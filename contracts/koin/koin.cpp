#include <koinos/system/system_calls.hpp>
#include <koinos/contracts/token/token.h>

#include <koinos/buffer.hpp>
#include <koinos/common.h>

#include <string>

using namespace koinos;
using namespace koinos::contracts;

#define KOINOS_NAME     "Test Koinos"
#define KOINOS_SYMBOL   "tKOIN"
#define KOINOS_DECIMALS 8

#define SUPPLY_KEY std::string( "" )

std::string contract_db_space = "";

constexpr std::size_t address_size = 25;

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

token::name_return< 32 > name()
{
   token::name_return< 32 > ret;
   ret.mutable_value() = KOINOS_NAME;
   return ret;
}

token::symbol_return< 8 > symbol()
{
   token::symbol_return< 8 > ret;
   ret.mutable_value() = KOINOS_SYMBOL;
   return ret;
}

token::decimals_return decimals()
{
   token::decimals_return ret;
   ret.mutable_value() = KOINOS_DECIMALS;
   return ret;
}

token::total_supply_return total_supply()
{
   token::total_supply_return ret;

   uint64_t supply = 0;
   system::get_object( contract_db_space, SUPPLY_KEY, supply );

   ret.mutable_value() = supply;
   return ret;
}

token::balance_of_return balance_of( const token::balance_of_args< address_size >& args )
{
   token::balance_of_return ret;

   uint64_t balance = 0;

   std::string owner( reinterpret_cast< const char* >( args.get_owner().get_const() ), args.get_owner().get_length() );
   system::get_object( contract_db_space, owner, balance );

   ret.mutable_value() = balance;
   return ret;
}

token::transfer_return transfer( const token::transfer_args< address_size, address_size >& args )
{
   token::transfer_return ret;

   std::string from( reinterpret_cast< const char* >( args.get_from().get_const() ), args.get_from().get_length() );
   std::string to( reinterpret_cast< const char* >( args.get_to().get_const() ), args.get_to().get_length() );
   uint64_t value = args.get_value();

   system::require_authority( from );

   token::balance_of_args< address_size > ba_args;
   ba_args.mutable_owner() = args.get_from();
   auto from_balance = balance_of( ba_args ).get_value();

   if ( from_balance < value )
   {
      ret.mutable_value() = false;
      return ret;
   }

   from_balance = from_balance - value;

   ba_args.mutable_owner() = args.get_to();
   auto to_balance = balance_of( ba_args ).get_value() + value;

   system::put_object( contract_db_space, from, from_balance );
   system::put_object( contract_db_space, to, to_balance );

   ret.mutable_value() = true;
   return ret;
}

token::mint_return mint( const token::mint_args< address_size >& args )
{
   token::mint_return ret;

   std::string to( reinterpret_cast< const char* >( args.get_to().get_const() ), args.get_to().get_length() );
   uint64_t amount = args.get_value();

   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      ret.mutable_value() = false;
      return ret;
   }

   auto supply = total_supply().get_value();
   auto new_supply = supply + amount;

   // Check overflow
   if ( new_supply < supply )
   {
      ret.mutable_value() = false;
      return ret;
   }

   token::balance_of_args< address_size > ba_args;
   ba_args.mutable_owner() = args.get_to();
   auto to_balance = balance_of( ba_args ).get_value() + amount;

   system::put_object( contract_db_space, SUPPLY_KEY, new_supply );
   system::put_object( contract_db_space, to, to_balance );

   ret.mutable_value() = true;
   return ret;
}

int main()
{
   auto entry_point = system::get_entry_point();
   auto args = system::get_contract_args();
   contract_db_space = system::get_contract_id();

   std::array< uint8_t, 2048 > retbuf;

   switch( std::underlying_type_t< entries >( entry_point ) )
   {
      case entries::name_entry:
      {
         auto ret = name();
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      case entries::symbol_entry:
      {
         auto ret = symbol();
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      case entries::decimals_entry:
      {
         auto ret = decimals();
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      case entries::total_supply_entry:
      {
         auto ret = total_supply();
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      case entries::balance_of_entry:
      {
         koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
         token::balance_of_args< address_size > arg;
         arg.deserialize( rdbuf );

         auto ret = balance_of( arg );
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      case entries::transfer_entry:
      {
         koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
         token::transfer_args< address_size, address_size > arg;
         arg.deserialize( rdbuf );

         auto ret = transfer( arg );
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      case entries::mint_entry:
      {
         koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
         token::mint_args< address_size > arg;
         arg.deserialize( rdbuf );

         auto ret = mint( arg );
         koinos::write_buffer buffer( retbuf.data(), retbuf.size() );
         ret.serialize( buffer );
         break;
      }
      default:
         system::exit_contract( 1 );
   }

   std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
   system::set_contract_return_bytes( retval );

   system::exit_contract( 0 );
   return 0;
}
