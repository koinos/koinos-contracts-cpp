#include <koinos/system/system_calls.hpp>

#include <koinos/contracts/resources/resources.h>
#include <koinos/token.hpp>

#include <boost/multiprecision/cpp_int.hpp>

#include <string>

using namespace koinos;
using namespace koinos::contracts::resources;
using namespace std::string_literals;

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
constexpr uint64_t compute_budget_per_block       = 57'500'000; // ~0.1s
constexpr uint64_t max_compute_per_block          = 287'500'000; // ~0.5s

#ifdef BUILD_FOR_TESTING
// Address 1BRmrUgtSQVUggoeE9weG4f7nidyydnYfQ
const std::string koin_contract               = "\x00\x72\x60\xae\xaf\xad\xc7\x04\x31\xea\x9c\x3f\xbe\xf1\x35\xb9\xa4\x15\xc1\x0f\x51\x95\xe8\xd5\x57"s;
#else
// Address 19JntSm8pSNETT9aHTwAUHC5RMoaSmgZPJ
const std::string koin_contract               = "\x00\x5b\x1e\x61\xd3\x72\x59\xb9\xc2\xd9\x9b\xf4\x17\xf5\x92\xe0\xb7\x77\x25\x16\x5d\x24\x88\xbe\x45"s;
#endif

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

#define DECAY_CONSTANT_MUL 0xd75a712f
#define DECAY_CONSTANT_SHIFT 53

#define PHANTOM_RC_CONSTANT_MUL 0x91a2b3c5
#define PHANTOM_RC_CONSTANT_SHIFT 45

// Multiply by a number less than 1 using integer multiplication followed by a shift.
// The result is guaranteed not to overflow, and the final cast is guaranteed to fit,
// as long as x, m are 64-bit, x*m does not overflow 127 bits,
// and m, s represent multiplication by a number smaller than 1.
#define MUL_SHIFT(x, m, s) \
   uint64_t(((m) * int128_t(x)) >> (s))

#define DECAY(x, m, s) \
   x -= MUL_SHIFT(x, m, s);

void update_market( market& m, uint64_t resources_consumed )
{
   // Use price times quantity to back-calculate how much RC users spent.
   auto [limit, cost] = calculate_market_limit( m );
   int128_t rc_consumed = resources_consumed;
   rc_consumed *= cost;

   // Per (7), decay should apply after resource consumption and before budget.
   uint64_t resource_supply = m.resource_supply();

   // TODO:  Check for subtraction overflow, or write a comment explaining caller must check for overflow
   resource_supply -= resources_consumed;
   DECAY(resource_supply, DECAY_CONSTANT_MUL, DECAY_CONSTANT_SHIFT);
   resource_supply += m.block_budget();

   uint64_t rc_reserve = m.rc_reserve();

   // Per (9), RC decay should apply before phantom RC or user consumption.
   DECAY(rc_reserve, DECAY_CONSTANT_MUL, DECAY_CONSTANT_SHIFT);
   int128_t rc_reserve_128 = int128_t(rc_reserve) + rc_consumed;

   auto koin_token = koinos::token( constants::koin_contract );

   uint64_t phantom_rc = koin_token.total_supply();
   phantom_rc = MUL_SHIFT( phantom_rc, PHANTOM_RC_CONSTANT_MUL, PHANTOM_RC_CONSTANT_SHIFT );
   rc_reserve_128 += phantom_rc;
   rc_reserve = uint64_t( std::min( (int128_t(1) << 64)-1, rc_reserve_128 ) );

   m.set_resource_supply( resource_supply );
   m.set_rc_reserve( rc_reserve );
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
   auto [entry_point, args] = system::get_arguments();

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

   system::result r;
   r.mutable_object().set( buffer.data(), buffer.get_size() );

   system::exit( 0, r );

   return 0;
}
