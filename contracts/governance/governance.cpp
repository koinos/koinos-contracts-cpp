#include <koinos/system/system_calls.hpp>
#include <koinos/contracts/governance/governance.h>
#include <koinos/token.hpp>

#include <koinos/buffer.hpp>
#include <koinos/common.h>

#include <string>

using namespace koinos;
using namespace koinos::contracts;

namespace constants {

const uint64_t proposal_space_id = 0;
const auto contract_id           = system::get_contract_id();
const std::string koin_contract  = "\x00\x5b\x1e\x61\xd3\x72\x59\xb9\xc2\xd9\x9b\xf4\x17\xf5\x92\xe0\xb7\x77\x25\x16\x5d\x24\x88\xbe\x45"s;

} // constants

namespace state {

namespace detail {

system::object_space create_proposal_space()
{
   system::object_space proposal_space;
   supply_space.mutable_zone().set( reinterpret_cast< const uint8_t* >( constants::contract_id.data() ), constants::contract_id.size() );
   supply_space.set_id( constants::proposal_space_id );
   supply_space.set_system( true );
   return proposal_space;
}

} // detail

const system::object_space& proposal_space()
{
   static const auto proposal_space = detail::create_proposal_space();
   return proposal_space;
}

} // state

enum entries : uint32_t
{
   submit_proposal_entry         = 0xe74b785c,
   get_proposal_by_id_entry      = 0xc66013ad,
   get_proposals_by_status_entry = 0x66206f76,
   get_proposals_entry           = 0xd44caa11,
   block_callback_entry          = 0x531d5d4e
};

submit_proposal_result submit_proposal( const submit_proposal_arguments& args )
{
   submit_proposal_result res;
   return res;
}

get_proposal_by_id_result get_proposal_by_id( const get_proposal_by_id& args )
{
   get_proposal_by_id_result res;
   return res;
}

get_proposal_by_status_result get_proposal_by_id( const get_proposal_by_status& args )
{
   get_proposal_by_status_result res;
   return res;
}

get_proposals_result get_proposals( const get_proposals_arguments& args )
{
   get_proposals_result res;
   return res;
}

block_callback_result block_callback( const block_callback_arguments& )
{
   block_callback_result res;
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
      case entries::submit_proposal_entry:
      {
         submit_proposal_arguments arg;
         arg.deserialize( rdbuf );

         auto res = submit_proposal( arg );
         res.serialize( buffer );
         break;
      }
      case entries::get_proposal_by_id_entry:
      {
         get_proposal_by_id_arguments arg;
         arg.deserialize( rdbuf );

         auto res = get_proposal_by_id( arg );
         res.serialize( buffer );
         break;
      }
      case entries::get_proposals_by_status_entry:
      {
         get_proposal_by_status_arguments arg;
         arg.deserialize( rdbuf );

         auto res = get_proposal_by_status( arg );
         res.serialize( buffer );
         break;
      }
      case entries::get_proposals_entry:
      {
         get_proposals_arguments arg;
         arg.deserialize( rdbuf );

         auto res = get_proposals( arg );
         res.serialize( buffer );
      }
      case entries::block_callback_entry:
      {
         block_callback_args args;
         block_callback( args );
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
