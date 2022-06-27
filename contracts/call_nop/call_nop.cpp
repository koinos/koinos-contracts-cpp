#include <koinos/system/system_calls.hpp>

using namespace koinos;

int main()
{
   uint32_t bytes_written = 0;

   invoke_system_call(
      std::underlying_type_t< chain::system_call_id >( chain::system_call_id::nop ),
      reinterpret_cast< char* >( system::detail::syscall_buffer.data() ),
      std::size( system::detail::syscall_buffer ),
      reinterpret_cast< char* >( system::detail::syscall_buffer.data() ),
      0,
      &bytes_written
   );

   system::exit( 0 );
}
