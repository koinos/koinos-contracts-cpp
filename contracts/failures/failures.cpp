#include <koinos/system/system_calls.hpp>
#include <stdio.h>
using namespace koinos;

int main()
{
   auto [ entry_point, args ] = system::get_arguments();

   std::array< uint8_t, 2048 > retbuf;

   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::write_buffer buffer( retbuf.data(), retbuf.size() );

   switch( entry_point )
   {
      // Divide by 0
      case 0x01:
      {
         system::log( "divide by zero" );
         volatile uint64_t a = 10;
         volatile uint64_t b = 0;
         system::log( std::to_string( a/b ) );
         break;
      }
      // Allocate too much memory (4GB+)
      case 0x02:
      {
         system::log( "allocate more than 4GB" );
         size_t allocSize = size_t(5) * size_t(1024) * size_t(1024) * size_t(1024);
         uint8_t* val = (uint8_t*)malloc( allocSize );
         char s[256];
         snprintf( s, sizeof( s ), "pointer: %p\n", (void*)val );
         system::log( std::string{ s } );
         memset( val, 1, sizeof(uint8_t) * allocSize);
         break;
      }
      // Infinite loop
      case 0x03:
      {
         while(1) {}
         break;
      }
      // Running out of stack
      case 0x04:
      {
         system::call( system::get_contract_id(), entry_point, std::string{} );
         break;
      }
      // Test behavior of floating point
      case 0x05:
      {
         float x = 1.3f;
         double y = 1.3f;
         system::log( std::to_string( x * y ) );
         break;
      }
      default:
         system::revert( "unknown entry point" );
   }

   system::result r;
   r.mutable_object().set( buffer.data(), buffer.get_size() );

   system::exit( 0, r );
}
