#include <koinos/system/system_calls.hpp>
#include <koinos/token.hpp>

using namespace koinos;

#define KOIN_CONTRACT uint160_t( "0x930f7a991f8787bd485b02684ffda52570439794" )
#define BLOCK_REWARD  10000000000 // 100 KOIN
#define TARGET_BLOCK_INTERVAL_MS 10000
#define DIFFICULTY_METADATA_KEY uint256_t( 0 )
#define GET_DIFFICULTY_ENTRYPOINT 0x4a758831
#define CRYPTO_SHA2_256_ID uint64_t(0x12)
#define POW_END_DATE 1640995199000 // 2021-12-31T23:59:59Z

#define HC_HALFLIFE_MS      uint64_t(60*60*24*1000)
#define FREE_RUN_SAT_MULT   2

/**
 * Diminishing returns bonus constant.
 */
#define DR_BONUS            uint64_t(0x1cd85d3da)

uint256_t contract_id;

struct pow_signature_data
{
   uint256_t        nonce;
   fixed_blob< 65 > recoverable_signature;
};

KOINOS_REFLECT( pow_signature_data, (nonce)(recoverable_signature) )

struct difficulty_metadata_v1
{
   uint256_t      target = 0;
   timestamp_type last_block_time = timestamp_type( 0 );
   timestamp_type block_window_time = timestamp_type( 0 );
   uint32_t       averaging_window = 0;
};

KOINOS_REFLECT( difficulty_metadata_v1,
   (target)
   (last_block_time)
   (block_window_time)
   (averaging_window)
)

struct difficulty_metadata_v2
{
   uint256_t      target = 0;
   timestamp_type last_block_time = timestamp_type( 0 );
   uint256_t      difficulty = 0;
   timestamp_type target_block_interval = timestamp_type( TARGET_BLOCK_INTERVAL_MS / 1000 );
};

KOINOS_REFLECT( difficulty_metadata_v2,
   (target)
   (last_block_time)
   (difficulty)
   (target_block_interval)
)

difficulty_metadata_v2 initialize_difficulty( const difficulty_metadata_v1& pow1_meta )
{
   difficulty_metadata_v2 diff_meta;
   diff_meta.target = pow1_meta.target;
   diff_meta.difficulty = std::numeric_limits< uint256_t >::max() / pow1_meta.target;
   diff_meta.last_block_time = pow1_meta.last_block_time;

   return diff_meta;
}

difficulty_metadata_v2 get_difficulty_meta()
{
   difficulty_metadata_v2 diff_meta;
   auto diff_meta_vb = system::db_get_object( contract_id, DIFFICULTY_METADATA_KEY );

   if ( diff_meta_vb.size() == sizeof( difficulty_metadata_v2 ) )
   {
      diff_meta = pack::from_variable_blob< difficulty_metadata_v2 >( diff_meta_vb );
   }
   else
   {
      auto pow1_meta = pack::from_variable_blob< difficulty_metadata_v1 >( diff_meta_vb );
      diff_meta = initialize_difficulty( pow1_meta );
   }

   return diff_meta;
}

void update_difficulty( difficulty_metadata_v2& diff_meta, timestamp_type current_block_time )
{
   diff_meta.difficulty = diff_meta.difficulty + diff_meta.difficulty / 2048 * std::max(1 - int64((current_block_time - diff_meta.last_block_time) / 7000), -99ll);
   diff_meta.last_block_time = current_block_time;
   diff_meta.target = std::numeric_limits< uint256_t >::max() / diff_meta.difficulty;

   system::db_put_object( contract_id, DIFFICULTY_METADATA_KEY, diff_meta );
}

int main()
{
   auto entry_point = system::get_entry_point();
   contract_id = pack::from_variable_blob< uint160_t >( pack::to_variable_blob( system::get_contract_id() ) );

   if ( entry_point == GET_DIFFICULTY_ENTRYPOINT )
   {
      system::set_contract_return( get_difficulty_meta() );
      system::exit_contract( 0 );
   }

   auto head_block_time = system::get_head_block_time();
   if ( head_block_time > POW_END_DATE )
   {
      system::print( "testnet has ended" );
      system::set_contract_return( false );
      system::exit_contract( 0 );
   }

   if ( system::get_caller().caller_privilege != chain::privilege::kernel_mode )
   {
      system::print( "pow contract must be called from kernel" );
      system::set_contract_return( false );
      system::exit_contract( 0 );
   }

   auto args = system::get_contract_args< chain::verify_block_signature_args >();
   auto signature_data = pack::from_variable_blob< pow_signature_data >( args.signature_data );
   auto to_hash = pack::to_variable_blob( signature_data.nonce );
   to_hash.insert( to_hash.end(), args.digest.digest.begin(), args.digest.digest.end() );

   auto pow = pack::from_variable_blob< uint256_t >( system::hash( CRYPTO_SHA2_256_ID, to_hash ).digest );

   // Get/update difficulty from database
   auto diff_meta = get_difficulty_meta();

   if ( pow > diff_meta.target )
   {
      system::print( "pow did not meet target\n" );
      system::set_contract_return( false );
      system::exit_contract( 0 );
   }

   update_difficulty( diff_meta, head_block_time );

   // Recover address from signature
   auto producer = system::recover_public_key( pack::to_variable_blob( signature_data.recoverable_signature ), args.digest );

   args.active_data.unbox();
   auto signer = args.active_data.get_const_native().signer;

   if ( producer != signer )
   {
      system::print( "signature and signer are mismatching\n" );
      system::set_contract_return( false );
      system::exit_contract( 0 );
   }

   // Mint block reward to address
   auto koin_token = koinos::token( KOIN_CONTRACT );

   auto success = koin_token.mint( signer, BLOCK_REWARD );
   if ( !success )
   {
      system::print( "could not mint KOIN to producer address " + std::string( producer.data(), producer.size() ) + '\n' );
   }

   system::set_contract_return( success );
   system::exit_contract( 0 );

   return 0;
}
