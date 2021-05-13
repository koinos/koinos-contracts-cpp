#include <koinos/system/system_calls.hpp>

#include <streambuf>

using namespace koinos;

//using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;

extern "C" void invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );

#define KOINOS_NAME     "Koinos"
#define KOINOS_SYMBOL   "KOIN"
#define KOINOS_DECIMALS 8

#define SUPPLY_KEY uint256( 0 )

uint32_t get_entry_point()
{
   variable_blob return_buffer(10);
   variable_blob args;

   invoke_system_call(
      (uint32_t)0x91208aee,
      return_buffer.data(),
      return_buffer.size(),
      args.data(),
      args.size()
   );

   return pack::from_variable_blob< uint32_t >( return_buffer );
}

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

template< typename T >
bool get_object( const uint256& key, T& t )
{
   auto vb = system::db_get_object( 0, key, sizeof( T ) );
   if( vb.size() )
   {
      t = pack::from_variable_blob< T >( vb );
      return true;
   }

   return false;
}

template< typename T >
bool put_object( const uint256& key, const T& value )
{
   return system::db_put_object( 0, key, pack::to_variable_blob( value ) );
}

struct transfer_args
{
   chain::account_type from;
   chain::account_type to;
   uint256             value;
};

KOINOS_REFLECT( transfer_args, (from)(to)(value) );

struct mint_args
{
   chain::account_type to;
   uint256             value;
};

KOINOS_REFLECT( mint_args, (to)(value) );

std::string name()
{
   return KOINOS_NAME;
}

std::string symbol()
{
   return KOINOS_SYMBOL;
}

uint8_t decimals()
{
   return KOINOS_DECIMALS;
}

uint256 total_supply()
{
   uint256 supply = 0;
   get_object< uint256 >( SUPPLY_KEY, supply );
   return supply;
}

uint256 balance_of( const chain::account_type& owner )
{
   uint256 balance = 0;
   get_object< uint256 >( pack::from_variable_blob< uint256 >( owner ), balance );
   return balance;
}

bool transfer( const chain::account_type& from, const chain::account_type& to, const uint256& value )
{
   system::require_authority( from );
   auto from_balance = balance_of( from );

   if ( from_balance < value ) return false;

   from_balance -= value;
   auto to_balance = balance_of( to ) + value;

   auto success = put_object( pack::from_variable_blob< uint256 >( from ), from_balance );
   if ( !success ) return false;

   return put_object( pack::from_variable_blob< uint256 >( to ), to_balance );
}

bool mint( const chain::account_type& to, const uint256& amount )
{
   // TODO: Authorization
   auto supply = total_supply();
   auto new_supply = supply + amount;

   if ( new_supply < supply ) return false; // Overflow detected

   auto success = put_object( SUPPLY_KEY, new_supply );
   if ( !success ) return false;

   auto to_balance = balance_of( to ) + amount;

   return put_object( pack::from_variable_blob< uint256 >( to ), to_balance );
}

int main()
{
   auto entry_point = get_entry_point();
   auto args = system::get_contract_args();

   variable_blob return_blob;

   switch( uint32_t(entry_point) )
   {
      case entries::name_entry:
      {
         return_blob = pack::to_variable_blob( name() );
         break;
      }
      case entries::symbol_entry:
      {
         return_blob = pack::to_variable_blob( symbol() );
         break;
      }
      case entries::decimals_entry:
      {
         return_blob = pack::to_variable_blob( decimals() );
         break;
      }
      case entries::total_supply_entry:
      {
         return_blob = pack::to_variable_blob( total_supply() );
         break;
      }
      case entries::balance_of_entry:
      {
         auto owner = pack::from_variable_blob< chain::account_type >( args );
         return_blob = pack::to_variable_blob( balance_of( owner ) );
         break;
      }
      case entries::transfer_entry:
      {
         auto t_args = pack::from_variable_blob< transfer_args >( args );
         return_blob = pack::to_variable_blob( transfer( t_args.from, t_args.to, t_args.value ) );
         break;
      }
      case entries::mint_entry:
      {
         auto m_args = pack::from_variable_blob< mint_args >( args );
         return_blob = pack::to_variable_blob( mint( m_args.to, m_args.value ) );
         break;
      }
      default:
         system::exit_contract( 1 );
   }

   system::set_contract_return( return_blob );

   system::exit_contract( 0 );
   return 0;
}

