#include <koinos/system/system_calls.hpp>

using namespace koinos;

#define KOIN_CONTRACT uint160_t( 0 )

int main()
{
   auto args = system::get_contract_args();
   variable_blob return_blob;


   system::set_contract_return( return_blob );

   system::exit_contract( 0 );
   return 0;
}
