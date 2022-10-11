#include <koinos/system/system_calls.hpp>
#include <koinos/token.hpp>

#include <koinos/contracts/resources/resources.h>

#include <boost/multiprecision/cpp_int.hpp>

using namespace koinos;
using namespace koinos::contracts::resources;
using namespace std::string_literals;

using uint128_t = boost::multiprecision::uint128_t;

enum entries : uint32_t
{
   get_resource_limits_entry             = 0x427a0394,
   consume_block_resources_entry         = 0x9850b1fd,
   get_resource_markets_entry            = 0xebe9b9e7,
   set_resource_markets_parameters_entry = 0x4b31e959,
   get_resource_parameters_entry         = 0xf53b5216,
   set_resource_parameters_entry         = 0xa08e6b90
};

namespace constants {

constexpr std::size_t max_buffer_size         = 2048;
constexpr uint64_t num_resources              = 3;
const std::string markets_key                 = "markets";
const std::string parameters_keys             = "parameters";

constexpr uint64_t disk_budget_per_block_default    = 39600; // 10G per month
constexpr uint64_t max_disk_per_block_default       = 200 << 10; // 200k
constexpr uint64_t network_budget_per_block_default = 1 << 18; // 256k block
constexpr uint64_t max_network_per_block_default    = 1 << 20; // 1M block
constexpr uint64_t compute_budget_per_block_default = 57'500'000; // ~0.1s
constexpr uint64_t max_compute_per_block_default    = 287'500'000; // ~0.5s

constexpr uint64_t block_interval_ms_default        = 3'000; // 3s
constexpr uint64_t rc_regen_ms_default              = 432'000'000; // 5 daysc

// Exponential decay constant for 1 month half life
// Constant is ( 2 ^ (-1 / num_blocks) ) * 2^64
constexpr uint64_t decay_constant_default           = 18446596084619782819ull;
constexpr uint64_t one_minus_decay_constant_default = 147989089768795ull;

constexpr uint64_t print_rate_premium_default       = 1688;
constexpr uint64_t print_rate_precision_default     = 1000;

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

uint64_t rc_per_block( const resource_parameters& p )
{
   static uint64_t rc = 0;

   if ( rc == 0 )
   {
      auto koin_token = koinos::token( constants::koin_contract );
      rc = ( ( uint128_t( koin_token.total_supply() ) * p.block_interval_ms() ) / ( p.rc_regen_ms() * constants::num_resources ) ).convert_to< uint64_t >();
   }

   return rc;
}

void initialize_params( resource_parameters& params )
{
   params.set_block_interval_ms( constants::block_interval_ms_default );
   params.set_rc_regen_ms( constants::rc_regen_ms_default );
   params.set_decay_constant( constants::decay_constant_default );
   params.set_one_minus_decay_constant( constants::one_minus_decay_constant_default );
   params.set_print_rate_premium( constants::print_rate_premium_default );
   params.set_print_rate_precision( constants::print_rate_precision_default );
}

resource_parameters get_resource_parameters()
{
   resource_parameters params;
   if ( !system::get_object( state::contract_space(), constants::parameters_keys, params ) )
   {
      initialize_params( params );
   }

   return params;
}

void initialize_markets( const resource_parameters& p, resource_markets& markets )
{
   auto print_rate = ( constants::disk_budget_per_block_default * p.print_rate_premium() ) / p.print_rate_precision();
   markets.mutable_disk_storage().set_resource_supply( ( ( uint128_t( print_rate ) << 64 ) / p.one_minus_decay_constant() ).convert_to< uint64_t >() );
   markets.mutable_disk_storage().set_block_budget( constants::disk_budget_per_block_default );
   markets.mutable_disk_storage().set_block_limit( constants::max_disk_per_block_default );

   print_rate = ( constants::network_budget_per_block_default * p.print_rate_premium() ) / p.print_rate_precision();
   markets.mutable_network_bandwidth().set_resource_supply( ( ( uint128_t( print_rate ) << 64 ) / p.one_minus_decay_constant() ).convert_to< uint64_t >() );
   markets.mutable_network_bandwidth().set_block_budget( constants::network_budget_per_block_default );
   markets.mutable_network_bandwidth().set_block_limit( constants::max_network_per_block_default );

   print_rate = ( constants::compute_budget_per_block_default * p.print_rate_premium() ) / p.print_rate_precision();
   markets.mutable_compute_bandwidth().set_resource_supply( ( ( uint128_t( print_rate ) << 64 ) / p.one_minus_decay_constant() ).convert_to< uint64_t >() );
   markets.mutable_compute_bandwidth().set_block_budget( constants::compute_budget_per_block_default );
   markets.mutable_compute_bandwidth().set_block_limit( constants::max_compute_per_block_default );
}

resource_markets get_resource_markets()
{
   resource_markets markets;
   if ( !system::get_object( state::contract_space(), constants::markets_key, markets ) )
   {
      initialize_markets( get_resource_parameters(), markets );
   }

   return markets;
}

void set_resource_markets( const set_resource_markets_parameters_arguments& params )
{
   if ( !system::check_system_authority() )
      system::fail( "can only set market parameters with system authority", chain::error_code::authorization_failure );

   auto markets = get_resource_markets();

   markets.mutable_disk_storage().set_block_budget( params.get_disk_storage().get_block_budget() );
   markets.mutable_disk_storage().set_block_limit( params.get_disk_storage().get_block_limit() );
   markets.mutable_network_bandwidth().set_block_budget( params.get_network_bandwidth().get_block_budget() );
   markets.mutable_network_bandwidth().set_block_limit( params.get_network_bandwidth().get_block_limit() );
   markets.mutable_compute_bandwidth().set_block_budget( params.get_compute_bandwidth().get_block_budget() );
   markets.mutable_compute_bandwidth().set_block_limit( params.get_compute_bandwidth().get_block_limit() );

   system::put_object( state::contract_space(), constants::markets_key, markets );
}

void set_resource_parameters( const set_resource_parameters_arguments& args )
{
   if ( !system::check_system_authority() )
      system::fail( "can only set resource parameters with system authority", chain::error_code::authorization_failure );

   system::put_object( state::contract_space(), constants::parameters_keys, args.get_params() );
}

uint128_t calculate_k( const resource_parameters& p, const market& m )
{
   auto block_print_rate = ( p.print_rate_premium() * m.block_budget() ) / p.print_rate_precision();
   auto max_resources = ( uint128_t( block_print_rate - m.block_budget() ) << 64 ) / p.one_minus_decay_constant();
   return ( ( ( rc_per_block( p ) * max_resources ) / m.block_budget() ) * ( max_resources - m.block_budget() ) ).convert_to< uint64_t >();
}

std::pair< uint64_t, uint64_t > calculate_market_limit( const resource_parameters& p, const market& m )
{
   auto resource_limit = std::min( m.resource_supply() - 1, m.block_limit() );
   auto k = calculate_k( p, m );
   auto new_supply = m.resource_supply() - resource_limit;
   auto consumed_rc = ( ( k + ( new_supply - 1 ) ) / new_supply ) - ( k / m.resource_supply() );
   auto rc_cost = ( ( consumed_rc + ( resource_limit - 1 ) ) / resource_limit ).convert_to< uint64_t >();
   return std::make_pair( resource_limit, rc_cost );
}

get_resource_limits_result get_resource_limits( const resource_parameters& p )
{
   auto markets = get_resource_markets();

   auto [disk_limit,    disk_cost]    = calculate_market_limit( p, markets.disk_storage() );
   auto [network_limit, network_cost] = calculate_market_limit( p, markets.network_bandwidth() );
   auto [compute_limit, compute_cost] = calculate_market_limit( p, markets.compute_bandwidth() );

   get_resource_limits_result res;
   res.mutable_value().set_disk_storage_limit( disk_limit );
   res.mutable_value().set_disk_storage_cost( disk_cost );
   res.mutable_value().set_network_bandwidth_limit( network_limit );
   res.mutable_value().set_network_bandwidth_cost( network_cost );
   res.mutable_value().set_compute_bandwidth_limit( compute_limit );
   res.mutable_value().set_compute_bandwidth_cost( compute_cost );

   return res;
}

void update_market( const resource_parameters& p, market& m, uint64_t consumed )
{
   auto print_rate = ( m.block_budget() * p.print_rate_premium() ) / p.print_rate_precision();
   auto resource_supply = ( uint128_t( m.resource_supply() ) * p.decay_constant() ) >> 64;
   m.set_resource_supply( resource_supply.convert_to< uint64_t >() + print_rate - consumed );
   //m.set_resource_supply( m.resource_supply() + print_rate - consumed );
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

   auto markets = get_resource_markets();
   auto params = get_resource_parameters();

   if (  markets.disk_storage().resource_supply()      <= args.disk_storage_consumed()
      || markets.network_bandwidth().resource_supply() <= args.network_bandwidth_consumed()
      || markets.compute_bandwidth().resource_supply() <= args.compute_bandwidth_consumed()
      || markets.disk_storage().block_limit()      <= args.disk_storage_consumed()
      || markets.network_bandwidth().block_limit() <= args.network_bandwidth_consumed()
      || markets.compute_bandwidth().block_limit() <= args.compute_bandwidth_consumed() )
   {
      return res;
   }

   update_market( params, markets.mutable_disk_storage(),      args.disk_storage_consumed() );
   update_market( params, markets.mutable_network_bandwidth(), args.network_bandwidth_consumed() );
   update_market( params, markets.mutable_compute_bandwidth(), args.compute_bandwidth_consumed() );

   system::put_object( state::contract_space(), constants::markets_key, markets );

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
         auto res = get_resource_limits( get_resource_parameters() );
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
         res.set_value( get_resource_markets() );
         res.serialize( buffer );
         break;
      }
      case entries::set_resource_markets_parameters_entry:
      {
         set_resource_markets_parameters_arguments arg;
         arg.deserialize( rdbuf );

         set_resource_markets( arg );
         break;
      }
      case entries::get_resource_parameters_entry:
      {
         get_resource_parameters_result res;
         res.set_value( get_resource_parameters() );
         res.serialize( buffer );
         break;
      }
      case entries::set_resource_parameters_entry:
      {
         set_resource_parameters_arguments arg;
         arg.deserialize( rdbuf );

         set_resource_parameters( arg );
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
