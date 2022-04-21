#include <koinos/system/system_calls.hpp>
#include <koinos/contracts/governance/governance.h>
#include <koinos/token.hpp>

#include <koinos/buffer.hpp>
#include <koinos/common.h>

#include <string>
#include <functional>

using namespace koinos;
using namespace koinos::contracts;

using namespace std::string_literals;

namespace constants {

const uint64_t proposal_space_id         = 0;
const auto contract_id                   = system::get_contract_id();

#ifdef BUILD_FOR_TESTING
// Address 1BRmrUgtSQVUggoeE9weG4f7nidyydnYfQ
const std::string koin_contract               = "\x00\x72\x60\xae\xaf\xad\xc7\x04\x31\xea\x9c\x3f\xbe\xf1\x35\xb9\xa4\x15\xc1\x0f\x51\x95\xe8\xd5\x57"s;
#else
// Address 19JntSm8pSNETT9aHTwAUHC5RMoaSmgZPJ
const std::string koin_contract               = "\x00\x5b\x1e\x61\xd3\x72\x59\xb9\xc2\xd9\x9b\xf4\x17\xf5\x92\xe0\xb7\x77\x25\x16\x5d\x24\x88\xbe\x45"s;
#endif

constexpr uint64_t blocks_per_week       = uint64_t( 604800 ) / uint64_t( 10 );
constexpr uint64_t review_period         = blocks_per_week;
constexpr uint64_t vote_period           = blocks_per_week * 2;
constexpr uint64_t application_delay     = blocks_per_week;
constexpr uint64_t governance_threshold  = 75;
constexpr uint64_t standard_threshold    = 60;

constexpr uint64_t minimum_proposal_denominator = 1000000;
constexpr uint64_t maximum_proposal_multiplier  = 10;

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
   block_callback_entry          = 0x531d5d4e,
   authorize_entry               = 0x4a2dbd90
};

const uint64_t max_proposal_limit = 10;

using block_callback_arguments = koinos::contracts::governance::block_callback_arguments;
using block_callback_result = koinos::contracts::governance::block_callback_result;

using proposal_record = koinos::contracts::governance::proposal_record<
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

using submit_proposal_result = koinos::contracts::governance::submit_proposal_result;
using get_proposal_by_id_result = koinos::contracts::governance::get_proposal_by_id_result<
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

using proposal_submission_event = koinos::contracts::governance::proposal_submission_event<
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

bool proposal_updates_governance( const koinos::system::transaction& proposal )
{
   for ( uint64_t i = 0; i < proposal.get_operations().get_length(); i++ )
   {
      const auto& op = proposal.get_operations().get_const( i );
      switch( op.get_which_op() )
      {
         case koinos::system::operation::FieldNumber::UPLOAD_CONTRACT:
         {
            const auto& upload_op = op.get_upload_contract();

            std::string contract_id( reinterpret_cast< const char* >( upload_op.get_contract_id().get_const() ), upload_op.get_contract_id().get_length() );

            // We're uploading the current governance contract
            if ( contract_id == system::get_contract_id() )
               return true;
         }
         break;

         case koinos::system::operation::FieldNumber::CALL_CONTRACT:
         break;

         case koinos::system::operation::FieldNumber::SET_SYSTEM_CALL:
         {
            const auto& set_system_call_op = op.get_set_system_call();

            std::vector< koinos::chain::system_call_id > governance_syscalls;
            governance_syscalls.push_back( koinos::chain::system_call_id::pre_block_callback );
            governance_syscalls.push_back( koinos::chain::system_call_id::require_system_authority );
            governance_syscalls.push_back( koinos::chain::system_call_id::apply_set_system_call_operation );
            governance_syscalls.push_back( koinos::chain::system_call_id::apply_set_system_contract_operation );

            for ( const auto& syscall : governance_syscalls )
               if ( set_system_call_op.get_call_id().get() == std::underlying_type_t< koinos::chain::system_call_id >( syscall ) )
                  return true;
         }
         break;

         case koinos::system::operation::FieldNumber::SET_SYSTEM_CONTRACT:
         {
            const auto& set_system_contract_op = op.get_set_system_contract();

            std::string contract_id( reinterpret_cast< const char* >( set_system_contract_op.get_contract_id().get_const() ), set_system_contract_op.get_contract_id().get_length() );

            // We're setting the system status of the governance contract
            if ( contract_id == system::get_contract_id() )
               return true;
         }
         break;

         default:
         break;
      }
   }

   return false;
}

std::vector< proposal_record > retrieve_proposals(
   std::function< bool( const proposal_record& ) > predicate = []( const proposal_record& ) { return true; },
   uint64_t limit = 0,
   std::string start_proposal = std::string{} )
{
   std::vector< proposal_record > proposals;

   // No limit
   if ( !limit )
      limit = std::numeric_limits< uint64_t >::max();

   auto obj_bytes = system::get_next_object( state::proposal_space(), start_proposal );
   koinos::chain::database_object< system::detail::max_argument_size, system::detail::max_key_size > obj;
   koinos::read_buffer rbuf( reinterpret_cast< const uint8_t* >( obj_bytes.data() ), obj_bytes.size() );

   obj.deserialize( rbuf );

   std::size_t num_proposals = 0;
   while ( obj.get_exists() )
   {
      koinos::read_buffer rdbuf( obj.get_value().get_const(), obj.get_value().get_length() );

      proposal_record prec;
      prec.deserialize( rdbuf );

      if ( predicate( prec ) )
      {
         proposals.push_back( std::move( prec ) );
         num_proposals++;

         if ( num_proposals >= limit )
            break;
      }

      std::string next_key( reinterpret_cast< const char* >( obj.get_key().get_const() ), obj.get_key().get_length() );
      obj_bytes = system::get_next_object( state::proposal_space(), next_key );

      koinos::read_buffer rbuf( reinterpret_cast< const uint8_t* >( obj_bytes.data() ), obj_bytes.size() );
      obj.deserialize( rbuf );
   }

   return proposals;
}

submit_proposal_result submit_proposal( const submit_proposal_arguments& args )
{
   submit_proposal_result res;
   res.set_value( false );

   auto payer = koinos::system::get_transaction_field( "header.payer" );

   // Burn proposal fee
   auto koin_token = koinos::token( constants::koin_contract );
   auto total_supply = koin_token.total_supply();

   auto fee = std::max( total_supply / constants::minimum_proposal_denominator, args.get_proposal().get_header().get_rc_limit() * constants::maximum_proposal_multiplier );
   if ( args.get_fee() < fee )
   {
      std::string err_msg = "Proposal fee threshold not met - ";
      err_msg += "threshold: " + std::to_string( fee ) + ", ";
      err_msg += "actual: " + std::to_string( args.get_fee() );
      system::log( err_msg );
      return res;
   }

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

   prec.set_proposal( args.get_proposal() );
   prec.set_vote_start_height( block_height.uint64_value() + constants::blocks_per_week );
   prec.set_vote_tally( 0 );
   prec.set_shall_authorize( false );
   prec.set_status( governance::proposal_status::pending );

   if ( proposal_updates_governance( prec.get_proposal() ) )
   {
      prec.set_updates_governance( true );
      prec.set_vote_threshold( constants::vote_period * constants::governance_threshold / 100 );
   }
   else
   {
      prec.set_updates_governance( false );
      prec.set_vote_threshold( constants::vote_period * constants::standard_threshold / 100 );
   }

   system::put_object( state::proposal_space(), id, prec );

   proposal_submission_event pevent;
   pevent.set_proposal( prec );
   koinos::system::event( "proposal.submission", pevent );

   res.set_value( true );
   return res;
}

get_proposal_by_id_result get_proposal_by_id( const get_proposal_by_id_arguments& args )
{
   get_proposal_by_id_result res;

   std::string id( reinterpret_cast< const char* >( args.get_proposal_id().get_const() ), args.get_proposal_id().get_length() );

   proposal_record prec;

   if ( !koinos::system::get_object( state::proposal_space(), id, prec ) )
   {
      koinos::system::log( "Unable to find proposal" );
      return res;
   }

   res.set_value( prec );

   return res;
}

get_proposals_by_status_result get_proposals_by_status( const get_proposals_by_status_arguments& args )
{
   get_proposals_by_status_result res;

   std::string start( reinterpret_cast< const char* >( args.get_start_proposal().get_const() ), args.get_start_proposal().get_length() );

   auto proposals = retrieve_proposals( [&]( const proposal_record& p ) { return args.get_status() == p.get_status(); }, args.get_limit(), start );

   for ( const auto& p : proposals )
      res.add_value( p );

   return res;
}

get_proposals_result get_proposals( const get_proposals_arguments& args )
{
   get_proposals_result res;

   std::string start( reinterpret_cast< const char* >( args.get_start_proposal().get_const() ), args.get_start_proposal().get_length() );

   auto proposals = retrieve_proposals( [&]( const proposal_record& p ) { return true; }, args.get_limit(), start );

   for ( const auto& p : proposals )
      res.add_value( p );

   return res;
}

void handle_pending_proposal( proposal_record& p, uint64_t current_height )
{
   if ( p.get_vote_start_height() != current_height )
      return;

   std::string id( reinterpret_cast< const char* >( p.get_proposal().get_id().get_const() ), p.get_proposal().get_id().get_length() );
   p.set_status( governance::proposal_status::active );
   system::put_object( state::proposal_space(), id, p );

   governance::proposal_status_event< system::detail::max_hash_size > pevent;
   pevent.set_id( p.get_proposal().get_id() );
   pevent.set_status( p.get_status() );
   system::event( "proposal.status", pevent );
}

void handle_active_proposal( proposal_record& p, uint64_t current_height )
{
   if ( p.get_vote_start_height() + constants::vote_period != current_height )
      return;

   std::string id( reinterpret_cast< const char* >( p.get_proposal().get_id().get_const() ), p.get_proposal().get_id().get_length() );

   if ( p.get_vote_tally() < p.get_vote_threshold() )
   {
      p.set_status( governance::proposal_status::expired );

      governance::proposal_status_event< system::detail::max_hash_size > pevent;
      pevent.set_id( p.get_proposal().get_id() );
      pevent.set_status( p.get_status() );
      system::event( "proposal.status", pevent );

      system::remove_object( state::proposal_space(), id );
   }
   else
   {
      p.set_status( governance::proposal_status::approved );
      system::put_object( state::proposal_space(), id, p );

      governance::proposal_status_event< system::detail::max_hash_size > pevent;
      pevent.set_id( p.get_proposal().get_id() );
      pevent.set_status( p.get_status() );
      system::event( "proposal.status", pevent );
   }
}

void handle_approved_proposal( proposal_record& p, uint64_t current_height )
{
   if ( p.get_vote_start_height() + constants::vote_period + constants::application_delay != current_height )
      return;

   std::string id( reinterpret_cast< const char* >( p.get_proposal().get_id().get_const() ), p.get_proposal().get_id().get_length() );

   p.set_shall_authorize( true );
   system::put_object( state::proposal_space(), id, p );

   system::apply_transaction( p.get_proposal() );
   p.set_status( governance::proposal_status::applied );

   system::remove_object( state::proposal_space(), id );

   governance::proposal_status_event< system::detail::max_hash_size > pevent;
   pevent.set_id( p.get_proposal().get_id() );
   pevent.set_status( p.get_status() );
   system::event( "proposal.status", pevent );
}

void handle_votes()
{
   auto proposal_votes_message = system::get_block_field( "header.approved_proposals" ).message_value();
   chain::list_type< 32, 64, system::detail::max_hash_size > proposal_votes;
   proposal_votes_message.UnpackTo( proposal_votes );

   for ( uint64_t i = 0 ; i < proposal_votes.get_values().get_length(); i++ )
   {
      auto proposal_vote = proposal_votes.get_values().get_const( i ).get_bytes_value();
      std::string id( reinterpret_cast< const char* >( proposal_vote.get_const() ), proposal_vote.get_length() );

      proposal_record prec;

      if ( !system::get_object( state::proposal_space(), id, prec ) )
         continue;

      if ( prec.get_status() != governance::proposal_status::active )
         continue;

      auto current_vote_tally = prec.get_vote_tally();
      if ( current_vote_tally != std::numeric_limits< uint64_t >::max() )
         prec.set_vote_tally( current_vote_tally + 1 );

      system::put_object( state::proposal_space(), id, prec );

      governance::proposal_vote_event< system::detail::max_hash_size > pevent;
      pevent.set_id( prec.get_proposal().get_id() );
      pevent.set_vote_tally( prec.get_vote_tally() );
      pevent.set_vote_threshold( prec.get_vote_threshold() );
      system::event( "proposal.vote", pevent );
   }
}

block_callback_result block_callback()
{
   block_callback_result res;
   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      system::log( "Governance contract block callback must be called from kernel" );
      system::exit_contract( 1 );
   }

   handle_votes();

   auto current_height = system::get_block_field( "header.height" ).uint64_value();
   auto proposals = retrieve_proposals();

   for ( auto& p : proposals )
   {
      switch ( p.get_status() )
      {
      case governance::proposal_status::pending:
         handle_pending_proposal( p, current_height );
         break;
      case governance::proposal_status::active:
         handle_active_proposal( p, current_height );
         break;
      case governance::proposal_status::approved:
         handle_approved_proposal( p, current_height );
         break;
      default:
         system::log( "Attempted to handle unexpected proposal status" );
         break;
      }
   }
   return res;
}

koinos::chain::authorize_result authorize( const koinos::chain::authorize_arguments< system::detail::max_hash_size >& args )
{
   koinos::chain::authorize_result res;
   res.set_value( false );

   if ( args.get_type() == koinos::chain::authorization_type::transaction_application )
   {
      auto transaction_id_bytes = koinos::system::get_transaction_field( "header.id" ).get_bytes_value();
      std::string transaction_id( reinterpret_cast< const char* >( transaction_id_bytes.get_const() ), transaction_id_bytes.get_length() );

      proposal_record prec;

      if ( koinos::system::get_object( state::proposal_space(), transaction_id, prec ) )
         if ( prec.shall_authorize() )
            res.set_value( true );
   }

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
         break;
      }
      case entries::block_callback_entry:
      {
         auto res = block_callback();
         res.serialize( buffer );
         break;
      }
      case entries::authorize_entry:
      {
         koinos::chain::authorize_arguments< system::detail::max_hash_size > args;
         args.deserialize( rdbuf );

         auto res = authorize( args );
         res.serialize( buffer );
         break;
      }
      default:
         system::log( "Governance contract called with unknown entry point" );
         system::exit_contract( 1 );
         break;
   }

   std::string retval( reinterpret_cast< const char* >( buffer.data() ), buffer.get_size() );
   system::set_contract_result_bytes( retval );

   system::exit_contract( 0 );
   return 0;
}
