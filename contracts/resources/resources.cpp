#include <koinos/system/system_calls.hpp>

#include <koinos/contracts/resources/resources.h>

#include <boost/multiprecision/cpp_int.hpp>

using namespace koinos;
using namespace koinos::contracts::resources;

using int128_t = boost::multiprecision::int128_t;

enum entries : uint32_t
{
   get_resource_limits_entry     = 0x427a0394,
   consume_block_resources_entry = 0x9850b1fd,
   get_resource_markets_entry    = 0xebe9b9e7
};

namespace constants {

constexpr std::size_t max_buffer_size         = 2048;
const std::string market_key                  = "market";

// These are tuned really low, but we want to test constraints with low trx volume
constexpr uint64_t target_blocks_per_week         = 60480;
constexpr uint64_t target_utilized_supply         = 10000000000000;

constexpr uint64_t disk_budget_per_block          = 39600; // 10G per month
constexpr uint64_t max_disk_per_block             = 200 << 10; // 200k
constexpr uint64_t network_budget_per_block       = 1 << 18; // 256k block
constexpr uint64_t max_network_per_block          = 1 << 20; // 1M block
constexpr uint64_t compute_budget_per_block       = 230'000'000; // ~0.1s
constexpr uint64_t max_compute_per_block          = 1'150'000'000; // ~0.5s

// Exponential decay constant for 1 month half life
// Constant is ( -ln(2) * num_blocks ) * 2^64
// constexpr uint64_t decay_constant                 = 18446694743881045523ull;

// Exponential decay constant for 1 week half life
constexpr uint64_t decay_constant                 = 18446532661087609961ull;

} // constants


namespace state {

namespace detail {

system::object_space create_contract_space()
{
   system::object_space obj_space;
   auto contract_id = system::get_contract_id();
   obj_space.mutable_zone().set( reinterpret_cast< const uint8_t* >( contract_id.data() ), contract_id.size() );
   obj_space.set_id( 0 );
   obj_space.set_system( true );
   return obj_space;
}

} // detail

system::object_space contract_space()
{
   static auto space = detail::create_contract_space();
   return space;
}

}

using get_resource_limits_result        = chain::get_resource_limits_result;
using consume_block_resources_arguments = chain::consume_block_resources_arguments;
using consume_block_resources_result    = chain::consume_block_resources_result;

void initialize_markets( resource_markets& markets )
{
   markets.mutable_disk_storage().set_resource_supply( constants::disk_budget_per_block * constants::target_blocks_per_week );
   markets.mutable_disk_storage().set_rc_reserve( constants::target_utilized_supply );
   markets.mutable_disk_storage().set_block_budget( constants::disk_budget_per_block );
   markets.mutable_disk_storage().set_block_limit( constants::max_disk_per_block );

   markets.mutable_network_bandwidth().set_resource_supply( constants::network_budget_per_block * constants::target_blocks_per_week );
   markets.mutable_network_bandwidth().set_rc_reserve( constants::target_utilized_supply );
   markets.mutable_network_bandwidth().set_block_budget( constants::network_budget_per_block );
   markets.mutable_network_bandwidth().set_block_limit( constants::max_network_per_block );

   markets.mutable_compute_bandwidth().set_resource_supply( constants::compute_budget_per_block * constants::target_blocks_per_week );
   markets.mutable_compute_bandwidth().set_rc_reserve( constants::target_utilized_supply );
   markets.mutable_compute_bandwidth().set_block_budget( constants::compute_budget_per_block );
   markets.mutable_compute_bandwidth().set_block_limit( constants::max_compute_per_block );
}

resource_markets get_markets()
{
   resource_markets markets;
   if ( !system::get_object( state::contract_space(), constants::market_key, markets ) )
   {
      initialize_markets( markets );
   }

   return markets;
}

std::pair< uint64_t, uint64_t > calculate_market_limit( const market& m )
{
   auto resource_limit = std::min( m.resource_supply() - 1, m.block_limit() );
   auto k = int128_t( m.resource_supply() ) * int128_t( m.rc_reserve() );
   auto new_supply = m.resource_supply() - resource_limit;
   auto consumed_rc = ( ( k + ( new_supply - 1 ) ) / new_supply ) - m.rc_reserve();
   auto rc_cost = ( ( consumed_rc + ( resource_limit - 1 ) ) / resource_limit ).convert_to< uint64_t >();
   return std::make_pair( resource_limit, rc_cost );
}

get_resource_limits_result get_resource_limits()
{
   auto markets = get_markets();

   auto [disk_limit,    disk_cost]    = calculate_market_limit( markets.disk_storage() );
   auto [network_limit, network_cost] = calculate_market_limit( markets.network_bandwidth() );
   auto [compute_limit, compute_cost] = calculate_market_limit( markets.compute_bandwidth() );

   get_resource_limits_result res;
   res.mutable_value().set_disk_storage_limit( disk_limit );
   res.mutable_value().set_disk_storage_cost( disk_cost );
   res.mutable_value().set_network_bandwidth_limit( network_limit );
   res.mutable_value().set_network_bandwidth_cost( network_cost );
   res.mutable_value().set_compute_bandwidth_limit( compute_limit );
   res.mutable_value().set_compute_bandwidth_cost( compute_cost );

   return res;
}

void update_market( market& m, uint64_t consumed )
{
   auto k = int128_t( m.resource_supply() ) * int128_t( m.rc_reserve() );
   m.set_resource_supply( m.resource_supply() + m.block_budget() - consumed );
   m.set_resource_supply( ( ( int128_t( m.resource_supply() ) * constants::decay_constant ) >> 64 ).convert_to< uint64_t >() );
   m.set_rc_reserve( ( ( k + ( m.resource_supply() - 1 ) ) / m.resource_supply() ).convert_to< uint64_t >() );
}

consume_block_resources_result consume_block_resources( const consume_block_resources_arguments& args )
{
   consume_block_resources_result res;
   res.set_value( false );

   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      system::log( "The system call consume_block_resources must be called from kernel context" );
      return res;
   }

   auto markets = get_markets();

   if (  markets.disk_storage().resource_supply()      <= args.disk_storage_consumed()
      || markets.network_bandwidth().resource_supply() <= args.network_bandwidth_consumed()
      || markets.compute_bandwidth().resource_supply() <= args.network_bandwidth_consumed() )
   {
      return res;
   }

   update_market( markets.mutable_disk_storage(),      args.disk_storage_consumed() );
   update_market( markets.mutable_network_bandwidth(), args.network_bandwidth_consumed() );
   update_market( markets.mutable_compute_bandwidth(), args.compute_bandwidth_consumed() );

   system::put_object( state::contract_space(), constants::market_key, markets );

   res.set_value( true );
   return res;
}

int main()
{
   auto entry_point = system::get_entry_point();
   auto args = system::get_contract_arguments();

   std::array< uint8_t, constants::max_buffer_size > retbuf;

   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::write_buffer buffer( retbuf.data(), retbuf.size() );

   switch( std::underlying_type_t< entries >( entry_point ) )
   {
      case entries::get_resource_limits_entry:
      {
         auto res = get_resource_limits();
         res.serialize( buffer );
         break;
      }
      case entries::consume_block_resources_entry:
      {
         consume_block_resources_arguments arg;
         arg.deserialize( rdbuf );

         auto res = consume_block_resources( arg );
         res.serialize( buffer );
         break;
      }
      case entries::get_resource_markets_entry:
      {
         get_resource_markets_result res;
         res.set_value( get_markets() );
         res.serialize( buffer );
         break;
      }
      default:
         return 1;
   }

   std::string retval( reinterpret_cast< const char* >( buffer.data() ), buffer.get_size() );
   system::set_contract_result_bytes( retval );

   return 0;
}
