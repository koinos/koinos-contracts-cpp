#include <koinos/system/system_calls.hpp>
#include <koinos/contracts/token/token.h>

#include <koinos/buffer.hpp>
#include <koinos/common.h>

#include <boost/multiprecision/cpp_int.hpp>

#include <string>

using namespace koinos;
using namespace koinos::contracts;

using namespace std::string_literals;

using int128_t = boost::multiprecision::int128_t;

namespace constants {

static const std::string koinos_name   = "Test Koinos";
static const std::string koinos_symbol = "tKOIN";
constexpr uint32_t koinos_decimals     = 8;
constexpr uint64_t mana_regen_time_ms  = 432'000'000; // 5 days
constexpr std::size_t max_address_size = 25;
constexpr std::size_t max_name_size    = 32;
constexpr std::size_t max_symbol_size  = 8;
constexpr std::size_t max_buffer_size  = 2048;
constexpr uint32_t supply_id           = 0;
constexpr uint32_t balance_id          = 1;
std::string supply_key                 = "";
const auto contract_id                 = system::get_contract_id();

// 0x003c1756c0acf3b692631246da147ba66947b790eed4069e63 (16UjW8AKzuuePiRwmkvVDjuZvT2xksXb8N)
const std::string governance_contract  = "\x00\x3c\x17\x56\xc0\xac\xf3\xb6\x92\x63\x12\x46\xda\x14\x7b\xa6\x69\x47\xb7\x90\xee\xd4\x06\x9e\x63"s;

} // constants

namespace state {

namespace detail {

system::object_space create_supply_space()
{
   system::object_space supply_space;
   supply_space.mutable_zone().set( reinterpret_cast< const uint8_t* >( constants::contract_id.data() ), constants::contract_id.size() );
   supply_space.set_id( constants::supply_id );
   supply_space.set_system( true );
   return supply_space;
}

system::object_space create_balance_space()
{
   system::object_space balance_space;
   balance_space.mutable_zone().set( reinterpret_cast< const uint8_t* >( constants::contract_id.data() ), constants::contract_id.size() );
   balance_space.set_id( constants::balance_id );
   balance_space.set_system( true );
   return balance_space;
}

} // detail

const system::object_space& supply_space()
{
   static const auto supply_space = detail::create_supply_space();
   return supply_space;
}

const system::object_space& balance_space()
{
   static const auto balance_space = detail::create_balance_space();
   return balance_space;
}

} // state

enum entries : uint32_t
{
   get_account_rc_entry     = 0x2d464aab,
   consume_account_rc_entry = 0x80e3f5c9,
   name_entry               = 0x82a3537f,
   symbol_entry             = 0xb76a7ca1,
   decimals_entry           = 0xee80fd2f,
   total_supply_entry       = 0xb0da3934,
   balance_of_entry         = 0x5c721497,
   transfer_entry           = 0x27f576ca,
   mint_entry               = 0xdc6f17bb,
   burn_entry               = 0x859facc5
};

using get_account_rc_arguments
   = chain::get_account_rc_arguments<
      constants::max_name_size
   >;

using consume_account_rc_arguments
   = chain::consume_account_rc_arguments<
      constants::max_name_size
   >;

void regenerate_mana( token::mana_balance_object& bal )
{
   auto head_block_time = system::get_head_info().head_block_time();
   auto delta = std::min( head_block_time - bal.last_mana_update(), constants::mana_regen_time_ms );
   if ( delta )
   {
      auto new_mana = bal.mana() + ( ( int128_t( delta ) * int128_t( bal.balance() ) ) / constants::mana_regen_time_ms ).convert_to< uint64_t >() ;
      bal.set_mana( std::min( new_mana, bal.balance() ) );
      bal.set_last_mana_update( head_block_time );
   }
}

chain::get_account_rc_result get_account_rc( const get_account_rc_arguments& args )
{
   std::string owner( reinterpret_cast< const char* >( args.get_account().get_const() ), args.get_account().get_length() );
   chain::get_account_rc_result res;

   if ( owner == constants::governance_contract )
   {
      res.set_value( std::numeric_limits< uint64_t >::max() );
      return res;
   }

   token::mana_balance_object bal_obj;
   system::get_object( state::balance_space(), owner, bal_obj );

   regenerate_mana( bal_obj );

   res.set_value( bal_obj.get_mana() );
   return res;
}

chain::consume_account_rc_result consume_account_rc( const consume_account_rc_arguments& args )
{
   chain::consume_account_rc_result res;
   res.set_value( false );

   const auto [caller, privilege] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      system::log( "The system call consume_account_rc must be called from kernel context" );
      return res;
   }

   std::string owner( reinterpret_cast< const char* >( args.get_account().get_const() ), args.get_account().get_length() );
   token::mana_balance_object bal_obj;
   system::get_object( state::balance_space(), owner, bal_obj );

   regenerate_mana( bal_obj );

   // Assumes mana cannot go negative...
   if ( bal_obj.mana() < args.value() )
   {
      system::log( "Account has insufficient mana for consumption" );
      return res;
   }

   bal_obj.set_mana( bal_obj.mana() - args.value() );

   system::put_object( state::balance_space(), owner, bal_obj );

   res.set_value( true );
   return res;
}

token::name_result< constants::max_name_size > name()
{
   token::name_result< constants::max_name_size > res;
   res.mutable_value() = constants::koinos_name.c_str();
   return res;
}

token::symbol_result< constants::max_symbol_size > symbol()
{
   token::symbol_result< constants::max_symbol_size > res;
   res.mutable_value() = constants::koinos_symbol.c_str();
   return res;
}

token::decimals_result decimals()
{
   token::decimals_result res;
   res.mutable_value() = constants::koinos_decimals;
   return res;
}

token::total_supply_result total_supply()
{
   token::total_supply_result res;

   token::balance_object bal_obj;
   system::get_object( state::supply_space(), constants::supply_key, bal_obj );

   res.mutable_value() = bal_obj.get_value();
   return res;
}

token::balance_of_result balance_of( const token::balance_of_arguments< constants::max_address_size >& args )
{
   token::balance_of_result res;

   std::string owner( reinterpret_cast< const char* >( args.get_owner().get_const() ), args.get_owner().get_length() );

   token::mana_balance_object bal_obj;
   system::get_object( state::balance_space(), owner, bal_obj );

   res.set_value( bal_obj.get_balance() );
   return res;
}

token::transfer_result transfer( const token::transfer_arguments< constants::max_address_size, constants::max_address_size >& args )
{
   token::transfer_result res;
   res.set_value( false );

   std::string from( reinterpret_cast< const char* >( args.get_from().get_const() ), args.get_from().get_length() );
   std::string to( reinterpret_cast< const char* >( args.get_to().get_const() ), args.get_to().get_length() );
   uint64_t value = args.get_value();

   if ( from == to )
   {
      system::log( "Cannot transfer to self" );
      return res;
   }

   const auto [ caller, privilege ] = system::get_caller();
   if ( caller != from )
   {
      system::require_authority( from );
   }

   token::mana_balance_object from_bal_obj;
   system::get_object( state::balance_space(), from, from_bal_obj );

   if ( from_bal_obj.balance() < value )
   {
      system::log( "Account 'from' has insufficient balance" );
      return res;
   }

   regenerate_mana( from_bal_obj );

   if ( from_bal_obj.mana() < value )
   {
      system::log( "Account 'from' has insufficient mana for transfer" );
      return res;
   }

   token::mana_balance_object to_bal_obj;
   system::get_object( state::balance_space(), to, to_bal_obj );

   regenerate_mana( to_bal_obj );

   from_bal_obj.set_balance( from_bal_obj.balance() - value );
   from_bal_obj.set_mana( from_bal_obj.mana() - value );
   to_bal_obj.set_balance( to_bal_obj.balance() + value );
   to_bal_obj.set_mana( to_bal_obj.mana() + value );

   system::put_object( state::balance_space(), from, from_bal_obj );
   system::put_object( state::balance_space(), to, to_bal_obj );

   token::transfer_event< constants::max_address_size, constants::max_address_size > transfer_event;
   transfer_event.mutable_from().set( args.get_from().get_const(), args.get_from().get_length() );
   transfer_event.mutable_to().set( args.get_to().get_const(), args.get_to().get_length() );
   transfer_event.set_value( args.get_value() );

   std::vector< std::string > impacted;
   impacted.push_back( to );
   impacted.push_back( from );
   koinos::system::event( "koin.transfer", transfer_event, impacted );

   res.set_value( true );
   return res;
}

token::mint_result mint( const token::mint_arguments< constants::max_address_size >& args )
{
   token::mint_result res;
   res.set_value( false );

   std::string to( reinterpret_cast< const char* >( args.get_to().get_const() ), args.get_to().get_length() );
   uint64_t amount = args.get_value();

   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
#ifdef BUILD_FOR_TESTING
      system::require_authority( constants::contract_id );
#else
      system::log( "Can only mint token from kernel context" );
      return res;
#endif
   }

   auto supply = total_supply().get_value();
   auto new_supply = supply + amount;

   // Check overflow
   if ( new_supply < supply )
   {
      system::log( "Mint would overflow supply" );
      return res;
   }

   token::mana_balance_object to_bal_obj;
   system::get_object( state::balance_space(), to, to_bal_obj );

   regenerate_mana( to_bal_obj );

   to_bal_obj.set_balance( to_bal_obj.balance() + amount );
   to_bal_obj.set_mana( to_bal_obj.mana() + amount );

   token::balance_object supply_obj;
   supply_obj.set_value( new_supply );

   system::put_object( state::supply_space(), constants::supply_key, supply_obj );
   system::put_object( state::balance_space(), to, to_bal_obj );

   token::mint_event< constants::max_address_size > mint_event;
   mint_event.mutable_to().set( args.get_to().get_const(), args.get_to().get_length() );
   mint_event.set_value( amount );

   std::vector< std::string > impacted;
   impacted.push_back( to );
   koinos::system::event( "koin.mint", mint_event, impacted );

   res.set_value( true );
   return res;
}

token::burn_result burn( const token::burn_arguments< constants::max_address_size >& args )
{
   token::burn_result res;
   res.set_value( false );

   std::string from( reinterpret_cast< const char* >( args.get_from().get_const() ), args.get_from().get_length() );
   uint64_t value = args.get_value();

   const auto [ caller, privilege ] = system::get_caller();
   if ( caller != from )
   {
      system::require_authority( from );
   }

   token::mana_balance_object from_bal_obj;
   system::get_object( state::balance_space(), from, from_bal_obj );

   if ( from_bal_obj.balance() < value )
   {
      system::log( "Account 'from' has insufficient balance" );
      return res;
   }

   regenerate_mana( from_bal_obj );

   if ( from_bal_obj.mana() < value )
   {
      system::log( "Account 'from' has insufficient mana for burn" );
      return res;
   }

   from_bal_obj.set_balance( from_bal_obj.balance() - value );
   from_bal_obj.set_mana( from_bal_obj.mana() - value );

   auto supply = total_supply().get_value();

   // Check underflow
   if ( value <= supply )
   {
      system::log( "Burn would underflow supply" );
      return res;
   }

   auto new_supply = supply - value;

   token::balance_object supply_obj;
   supply_obj.set_value( new_supply );

   system::put_object( state::supply_space(), constants::supply_key, supply_obj );
   system::put_object( state::balance_space(), from, from_bal_obj );

   token::burn_event< constants::max_address_size > burn_event;
   burn_event.mutable_from().set( args.get_from().get_const(), args.get_from().get_length() );
   burn_event.set_value( args.get_value() );

   std::vector< std::string > impacted;
   impacted.push_back( from );
   koinos::system::event( "koin.burn", burn_event, impacted );

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
      case entries::get_account_rc_entry:
      {
         get_account_rc_arguments arg;
         arg.deserialize( rdbuf );

         auto res = get_account_rc( arg );
         res.serialize( buffer );
         break;
      }
      case entries::consume_account_rc_entry:
      {
         consume_account_rc_arguments arg;
         arg.deserialize( rdbuf );

         auto res = consume_account_rc( arg );
         res.serialize( buffer );
         break;
      }
      case entries::name_entry:
      {
         auto res = name();
         res.serialize( buffer );
         break;
      }
      case entries::symbol_entry:
      {
         auto res = symbol();
         res.serialize( buffer );
         break;
      }
      case entries::decimals_entry:
      {
         auto res = decimals();
         res.serialize( buffer );
         break;
      }
      case entries::total_supply_entry:
      {
         auto res = total_supply();
         res.serialize( buffer );
         break;
      }
      case entries::balance_of_entry:
      {
         token::balance_of_arguments< constants::max_address_size > arg;
         arg.deserialize( rdbuf );

         auto res = balance_of( arg );
         res.serialize( buffer );
         break;
      }
      case entries::transfer_entry:
      {
         token::transfer_arguments< constants::max_address_size, constants::max_address_size > arg;
         arg.deserialize( rdbuf );

         auto res = transfer( arg );
         res.serialize( buffer );
         break;
      }
      case entries::mint_entry:
      {
         token::mint_arguments< constants::max_address_size > arg;
         arg.deserialize( rdbuf );

         auto res = mint( arg );
         res.serialize( buffer );
         break;
      }
      case entries::burn_entry:
      {
         token::burn_arguments< constants::max_address_size > arg;
         arg.deserialize( rdbuf );

         auto res = burn( arg );
         res.serialize( buffer );
         break;
      }
      default:
         system::exit_contract( 1 );
   }

   std::string retval( reinterpret_cast< const char* >( buffer.data() ), buffer.get_size() );
   system::set_contract_result_bytes( retval );

   system::exit_contract( 0 );
   return 0;
}
