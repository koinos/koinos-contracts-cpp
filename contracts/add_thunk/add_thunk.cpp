#include <koinos/system/system_calls.hpp>

using namespace koinos;
using namespace std::string_literals;

namespace constants {

const uint64_t proposal_space_id       = 0;
const auto contract_id                 = system::get_contract_id();
const std::string compute_registry_key = "\x12\x20\xc5\x4f\xe8\x71\xc0\x9e\x87\x25\x0f\xc5\x0f\xd1\x16\xcc\xc3\xe9\xc0\xfd\xdb\x61\x36\x82\x43\x5a\xf5\xa0\x07\xf5\x54\xaf\x87\xc2";
const std::string thunk_name           = "nop";

} // constants

namespace state {

namespace detail {

system::object_space create_called_space()
{
   system::object_space called_space;
   called_space.mutable_zone().set( reinterpret_cast< const uint8_t* >( constants::contract_id.data() ), constants::contract_id.size() );
   called_space.set_id( constants::proposal_space_id );
   called_space.set_system( true );
   return called_space;
}

} // detail

const system::object_space& called_space()
{
   static const auto called_space = detail::create_called_space();
   return called_space;
}

} // state

int main()
{
   if ( auto record = system::detail::get_object( state::called_space(), 0 ); record.size() > 0 )
   {
      system::revert( "nop cost has already been added to compute bandwidth registry" );
   }

   system::object_space meta_space;
   meta_space.set_system( true );

   chain::compute_bandwidth_registry< 128, 64 > registry;

   if ( !system::get_object( meta_space, constants::compute_registry_key, registry ) )
   {
      system::revert( "could not find compute bandwidth registry" );
   }

   chain::compute_bandwidth_entry< 64 > entry;
   entry.mutable_name().set( constants::thunk_name.data(), constants::thunk_name.size() );
   entry.set_compute( 1 );

   registry.add_entries( entry );

   system::put_object( meta_space, constants::compute_registry_key, registry );
   system::detail::put_object( state::called_space(), 0, "\1"s );

   system::exit( 0 );
}
