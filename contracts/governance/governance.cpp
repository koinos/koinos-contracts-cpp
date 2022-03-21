#include <koinos/system/system_calls.hpp>
#include <koinos/contracts/governance/governance.h>
#include <koinos/token.hpp>

#include <koinos/buffer.hpp>
#include <koinos/common.h>

#include <string>

using namespace koinos;
using namespace koinos::contracts;

using namespace std::string_literals;

namespace constants {

const uint64_t proposal_space_id   = 0;
const auto contract_id             = system::get_contract_id();
const std::string koin_contract    = "\x00\x5b\x1e\x61\xd3\x72\x59\xb9\xc2\xd9\x9b\xf4\x17\xf5\x92\xe0\xb7\x77\x25\x16\x5d\x24\x88\xbe\x45"s;
constexpr uint64_t blocks_per_week = uint64_t( 604800 ) / uint64_t( 10 );
} // constants

namespace state {

namespace detail {

system::object_space create_proposal_space()
{
   system::object_space proposal_space;
   proposal_space.mutable_zone().set( reinterpret_cast< const uint8_t* >( constants::contract_id.data() ), constants::contract_id.size() );
   proposal_space.set_id( constants::proposal_space_id );
   proposal_space.set_system( true );
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

const uint64_t max_proposal_limit = 10;

using proposal_record = koinos::contracts::governance::proposal_record<
   system::detail::max_hash_size,           // id
   system::detail::max_hash_size,           // proposal.id
   system::detail::max_hash_size,           // proposal.header.chain_id
   system::detail::max_nonce_size,          // proposal.header.nonce
   system::detail::max_hash_size,           // proposal.header.operation_merkle_root
   system::detail::max_address_size,        // proposal.header.payer
   system::detail::max_address_size,        // proposal.header.payee
   system::detail::max_operation_length,    // proposal.operations length
   system::detail::max_address_size,        // proposal.upload_contract.contract_id
   system::detail::max_contract_size,       // proposal.upload_contract.bytecode
   system::detail::max_contract_size,       // proposal.upload_contract.abi
   system::detail::max_address_size,        // proposal.call_contract.contract_id
   system::detail::max_argument_size,       // proposal.call_contract.args
   system::detail::max_argument_size,       // proposal.set_system_call.target.system_call_bundle.contract_id
   system::detail::max_address_size,        // proposal.set_system_contract.contract_id
   system::detail::max_signatures_length,   // proposal.signatures length
   system::detail::max_signature_size >;    // proposal.signatures

using submit_proposal_arguments = koinos::contracts::governance::submit_proposal_arguments<
   system::detail::max_hash_size,           // id
   system::detail::max_hash_size,           // header.chain_id
   system::detail::max_nonce_size,          // header.nonce
   system::detail::max_hash_size,           // header.operation_merkle_root
   system::detail::max_address_size,        // header.payer
   system::detail::max_address_size,        // header.payee
   system::detail::max_operation_length,    // operations length
   system::detail::max_address_size,        // upload_contract.contract_id
   system::detail::max_contract_size,       // upload_contract.bytecode
   system::detail::max_contract_size,       // upload_contract.abi
   system::detail::max_address_size,        // call_contract.contract_id
   system::detail::max_argument_size,       // call_contract.args
   system::detail::max_argument_size,       // set_system_call.target.system_call_bundle.contract_id
   system::detail::max_address_size,        // set_system_contract.contract_id
   system::detail::max_signatures_length,   // signatures length
   system::detail::max_signature_size >;    // signatures

using get_proposal_by_id_arguments = koinos::contracts::governance::get_proposal_by_id_arguments< system::detail::max_hash_size >;
using get_proposals_by_status_arguments = koinos::contracts::governance::get_proposals_by_status_arguments< system::detail::max_hash_size >;
using get_proposals_arguments = koinos::contracts::governance::get_proposals_arguments< system::detail::max_hash_size >;

using block_callback_arguments = koinos::contracts::governance::block_callback_arguments;

using submit_proposal_result = koinos::contracts::governance::submit_proposal_result;
using get_proposal_by_id_result = koinos::contracts::governance::get_proposal_by_id_result<
   system::detail::max_hash_size,           // id
   system::detail::max_hash_size,           // proposal.id
   system::detail::max_hash_size,           // proposal.header.chain_id
   system::detail::max_nonce_size,          // proposal.header.nonce
   system::detail::max_hash_size,           // proposal.header.operation_merkle_root
   system::detail::max_address_size,        // proposal.header.payer
   system::detail::max_address_size,        // proposal.header.payee
   system::detail::max_operation_length,    // proposal.operations length
   system::detail::max_address_size,        // proposal.upload_contract.contract_id
   system::detail::max_contract_size,       // proposal.upload_contract.bytecode
   system::detail::max_contract_size,       // proposal.upload_contract.abi
   system::detail::max_address_size,        // proposal.call_contract.contract_id
   system::detail::max_argument_size,       // proposal.call_contract.args
   system::detail::max_argument_size,       // proposal.set_system_call.target.system_call_bundle.contract_id
   system::detail::max_address_size,        // proposal.set_system_contract.contract_id
   system::detail::max_signatures_length,   // proposal.signatures length
   system::detail::max_signature_size >;    // proposal.signatures

using get_proposals_by_status_result = koinos::contracts::governance::get_proposals_by_status_result<
   max_proposal_limit,
   system::detail::max_hash_size,           // id
   system::detail::max_hash_size,           // proposal.id
   system::detail::max_hash_size,           // proposal.header.chain_id
   system::detail::max_nonce_size,          // proposal.header.nonce
   system::detail::max_hash_size,           // proposal.header.operation_merkle_root
   system::detail::max_address_size,        // proposal.header.payer
   system::detail::max_address_size,        // proposal.header.payee
   system::detail::max_operation_length,    // proposal.operations length
   system::detail::max_address_size,        // proposal.upload_contract.contract_id
   system::detail::max_contract_size,       // proposal.upload_contract.bytecode
   system::detail::max_contract_size,       // proposal.upload_contract.abi
   system::detail::max_address_size,        // proposal.call_contract.contract_id
   system::detail::max_argument_size,       // proposal.call_contract.args
   system::detail::max_argument_size,       // proposal.set_system_call.target.system_call_bundle.contract_id
   system::detail::max_address_size,        // proposal.set_system_contract.contract_id
   system::detail::max_signatures_length,   // proposal.signatures length
   system::detail::max_signature_size >;    // proposal.signatures

using get_proposals_by_status_result = koinos::contracts::governance::get_proposals_by_status_result<
   max_proposal_limit,
   system::detail::max_hash_size,           // id
   system::detail::max_hash_size,           // proposal.id
   system::detail::max_hash_size,           // proposal.header.chain_id
   system::detail::max_nonce_size,          // proposal.header.nonce
   system::detail::max_hash_size,           // proposal.header.operation_merkle_root
   system::detail::max_address_size,        // proposal.header.payer
   system::detail::max_address_size,        // proposal.header.payee
   system::detail::max_operation_length,    // proposal.operations length
   system::detail::max_address_size,        // proposal.upload_contract.contract_id
   system::detail::max_contract_size,       // proposal.upload_contract.bytecode
   system::detail::max_contract_size,       // proposal.upload_contract.abi
   system::detail::max_address_size,        // proposal.call_contract.contract_id
   system::detail::max_argument_size,       // proposal.call_contract.args
   system::detail::max_argument_size,       // proposal.set_system_call.target.system_call_bundle.contract_id
   system::detail::max_address_size,        // proposal.set_system_contract.contract_id
   system::detail::max_signatures_length,   // proposal.signatures length
   system::detail::max_signature_size >;    // proposal.signatures

using get_proposals_result = koinos::contracts::governance::get_proposals_result<
   max_proposal_limit,
   system::detail::max_hash_size,           // id
   system::detail::max_hash_size,           // proposal.id
   system::detail::max_hash_size,           // proposal.header.chain_id
   system::detail::max_nonce_size,          // proposal.header.nonce
   system::detail::max_hash_size,           // proposal.header.operation_merkle_root
   system::detail::max_address_size,        // proposal.header.payer
   system::detail::max_address_size,        // proposal.header.payee
   system::detail::max_operation_length,    // proposal.operations length
   system::detail::max_address_size,        // proposal.upload_contract.contract_id
   system::detail::max_contract_size,       // proposal.upload_contract.bytecode
   system::detail::max_contract_size,       // proposal.upload_contract.abi
   system::detail::max_address_size,        // proposal.call_contract.contract_id
   system::detail::max_argument_size,       // proposal.call_contract.args
   system::detail::max_argument_size,       // proposal.set_system_call.target.system_call_bundle.contract_id
   system::detail::max_address_size,        // proposal.set_system_contract.contract_id
   system::detail::max_signatures_length,   // proposal.signatures length
   system::detail::max_signature_size >;    // proposal.signatures

using block_callback_result = koinos::contracts::governance::block_callback_result;

submit_proposal_result submit_proposal( const submit_proposal_arguments& args )
{
   submit_proposal_result res;
   res.set_value( false );

   auto payer = koinos::system::get_transaction_field( "header.payer" );

   // Burn proposal fee
   auto koin_token = koinos::token( constants::koin_contract );

   if ( !koin_token.burn( payer.string_value(), args.get_fee() ) )
   {
      system::log( "Could not burn KOIN for proposal submission" );
      return res;
   }

   std::string id( reinterpret_cast< const char* >( args.get_proposal().get_id().get_const() ), args.get_proposal().get_id().get_length() );

   auto block_height = koinos::system::get_block_field( "header.height" );

   proposal_record prec;

   if ( system::get_object( state::proposal_space(), id, prec ) )
   {
      system::log( "Proposal exists and cannot be updated" );
      return res;
   }

   prec.set_id( args.get_proposal().get_id() );
   prec.set_proposal( args.get_proposal() );
   prec.set_vote_start_height( block_height.uint64_value() + constants::blocks_per_week );
   prec.set_vote_tally( 0 );
   prec.set_shall_authorize( false );
   prec.set_status( governance::proposal_status::pending );

   if ( !system::put_object( state::proposal_space(), id, prec ) )
   {
      system::log( "Unable to store proposal in database" );
      return res;
   }

   res.set_value( true );
   return res;
}

get_proposal_by_id_result get_proposal_by_id( const get_proposal_by_id_arguments& args )
{
   get_proposal_by_id_result res;
   return res;
}

get_proposals_by_status_result get_proposals_by_status( const get_proposals_by_status_arguments& args )
{
   get_proposals_by_status_result res;
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

   std::array< uint8_t, koinos::system::detail::max_buffer_size > retbuf;

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
         get_proposals_by_status_arguments arg;
         arg.deserialize( rdbuf );

         auto res = get_proposals_by_status( arg );
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
         block_callback_arguments args;
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
