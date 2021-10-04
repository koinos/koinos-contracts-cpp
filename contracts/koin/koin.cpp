#include <koinos/system/system_calls.hpp>
#include <koinos/contracts/token/token.h>

#include <koinos/buffer.hpp>
#include <koinos/common.h>

#include <string>

using namespace koinos;
using namespace koinos::contracts;

namespace constants {

static const std::string koinos_name   = "Test Koinos";
static const std::string koinos_symbol = "tKOIN";
constexpr uint32_t koinos_decimals     = 8;
constexpr std::size_t max_address_size = 25;
constexpr std::size_t max_name_size    = 32;
constexpr std::size_t max_symbol_size  = 8;
constexpr std::size_t max_buffer_size  = 2048;
std::string supply_key                 = "";
std::string contract_space             = system::get_contract_id();

} // constants

enum entries : uint32_t
{
   name_entry         = 0x76ea4297,
   symbol_entry       = 0x7e794b24,
   decimals_entry     = 0x59dc15ce,
   total_supply_entry = 0xcf2e8212,
   balance_of_entry   = 0x15619248,
   transfer_entry     = 0x62efa292,
   mint_entry         = 0xc2f82bdc
};

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
   system::get_object( constants::contract_space, constants::supply_key, bal_obj );

   res.mutable_value() = bal_obj.get_value();
   return res;
}

token::balance_of_result balance_of( const token::balance_of_arguments< constants::max_address_size >& args )
{
   token::balance_of_result res;

   std::string owner( reinterpret_cast< const char* >( args.get_owner().get_const() ), args.get_owner().get_length() );

   token::balance_object bal_obj;
   system::get_object( constants::contract_space, owner, bal_obj );

   res.mutable_value() = bal_obj.get_value();
   return res;
}

token::transfer_result transfer( const token::transfer_arguments< constants::max_address_size, constants::max_address_size >& args )
{
   token::transfer_result res;

   std::string from( reinterpret_cast< const char* >( args.get_from().get_const() ), args.get_from().get_length() );
   std::string to( reinterpret_cast< const char* >( args.get_to().get_const() ), args.get_to().get_length() );
   uint64_t value = args.get_value();

   system::require_authority( from );

   token::balance_of_arguments< constants::max_address_size > ba_args;
   ba_args.mutable_owner() = args.get_from();
   auto from_balance = balance_of( ba_args ).get_value();

   if ( from_balance < value )
   {
      res.mutable_value() = false;
      return res;
   }

   from_balance = from_balance - value;

   ba_args.mutable_owner() = args.get_to();
   auto to_balance = balance_of( ba_args ).get_value() + value;

   token::balance_object bal_obj;

   bal_obj.mutable_value() = from_balance;
   system::put_object( constants::contract_space, from, bal_obj );

   bal_obj.mutable_value() = to_balance;
   system::put_object( constants::contract_space, to, bal_obj );

   res.mutable_value() = true;
   return res;
}

token::mint_result mint( const token::mint_arguments< constants::max_address_size >& args )
{
   token::mint_result res;

   std::string to( reinterpret_cast< const char* >( args.get_to().get_const() ), args.get_to().get_length() );
   uint64_t amount = args.get_value();

   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      res.mutable_value() = false;
      return res;
   }

   auto supply = total_supply().get_value();
   auto new_supply = supply + amount;

   // Check overflow
   if ( new_supply < supply )
   {
      res.mutable_value() = false;
      return res;
   }

   token::balance_of_arguments< constants::max_address_size > ba_args;
   ba_args.mutable_owner() = args.get_to();
   auto to_balance = balance_of( ba_args ).get_value() + amount;

   token::balance_object bal_obj;

   bal_obj.mutable_value() = new_supply;
   system::put_object( constants::contract_space, constants::supply_key, bal_obj );

   bal_obj.mutable_value() = to_balance;
   system::put_object( constants::contract_space, to, bal_obj );

   res.mutable_value() = true;
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
      default:
         system::exit_contract( 1 );
   }

   std::string retval( reinterpret_cast< const char* >( buffer.data() ), buffer.get_size() );
   system::set_contract_result_bytes( retval );

   system::exit_contract( 0 );
   return 0;
}
