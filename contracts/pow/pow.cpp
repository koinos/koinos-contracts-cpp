#include <koinos/system/system_calls.hpp>
#include <koinos/token.hpp>

using namespace koinos;

#define KOIN_CONTRACT uint160_t( 0 )
#define BLOCK_REWARD  10000000000 // 100 KOIN

#define TARGET_BLOCK_INTERVAL_MS 10000
#define UPDATE_DIFFICULTY_DELTA_MS 60000
#define BLOCK_AVERAGING_WINDOW   8640  // ~1 day of blocks

#define DIFFICULTY_METADATA_KEY uint256_t( 0 )

#define GET_DIFFICULTY_ENTRYPOINT 0x4a758831

#define CRYPTO_SHA2_256_ID uint64_t(0x12)

uint256_t contract_id;

struct pow_signature_data
{
   uint256_t        nonce;
   fixed_blob< 65 > recoverable_signature;
};

KOINOS_REFLECT( pow_signature_data, (nonce)(recoverable_signature) )

struct difficulty_metadata
{
   uint256_t      current_difficulty = 0;
   timestamp_type last_block_time = timestamp_type( 0 );
   timestamp_type block_window_time = timestamp_type( 0 );
   uint32_t       averaging_window = 0;
};

KOINOS_REFLECT( difficulty_metadata,
   (current_difficulty)
   (last_block_time)
   (block_window_time)
   (averaging_window)
)

struct mint_args
{
   chain::account_type to;
   uint64_t            value;
};

KOINOS_REFLECT( mint_args, (to)(value) );

difficulty_metadata get_difficulty_meta()
{
   difficulty_metadata diff_meta;
   system::db_get_object( contract_id, DIFFICULTY_METADATA_KEY, diff_meta );
   if ( diff_meta.current_difficulty == 0 )
   {
      diff_meta.current_difficulty = std::numeric_limits< uint256_t >::max() >> 22;
   }

   return diff_meta;
}

uint256_t get_and_update_difficulty( timestamp_type current_block_time )
{
   auto diff_meta = get_difficulty_meta();
   auto old_difficulty = diff_meta.current_difficulty;

   if ( diff_meta.last_block_time )
   {
      if ( diff_meta.averaging_window >= BLOCK_AVERAGING_WINDOW )
      {
         // Decay block window time
         diff_meta.block_window_time = diff_meta.block_window_time * (diff_meta.averaging_window - 1) / diff_meta.averaging_window;
      }
      else
      {
         diff_meta.averaging_window++;
      }

      diff_meta.block_window_time += current_block_time - diff_meta.last_block_time;

      if ( current_block_time / UPDATE_DIFFICULTY_DELTA_MS > diff_meta.last_block_time / UPDATE_DIFFICULTY_DELTA_MS )
      {
         uint64_t average_block_interval = diff_meta.block_window_time / diff_meta.averaging_window;

         if ( average_block_interval < TARGET_BLOCK_INTERVAL_MS )
         {
            auto block_time_diff = ( TARGET_BLOCK_INTERVAL_MS - average_block_interval );

            // This is a loss of precision, but it is a 256bit value, we should be fine
            diff_meta.current_difficulty -= ( diff_meta.current_difficulty / ( TARGET_BLOCK_INTERVAL_MS * 60 ) ) * block_time_diff;
         }
         else if ( average_block_interval > TARGET_BLOCK_INTERVAL_MS )
         {
            auto block_time_diff = ( average_block_interval - TARGET_BLOCK_INTERVAL_MS );

            // This is a loss of precision, but it is a 256bit value, we should be fine
            diff_meta.current_difficulty += ( diff_meta.current_difficulty / ( TARGET_BLOCK_INTERVAL_MS * 60 ) ) * block_time_diff;
         }
      }
   }

   diff_meta.last_block_time = current_block_time;

   system::db_put_object( contract_id, DIFFICULTY_METADATA_KEY, diff_meta );

   return old_difficulty;
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

   auto args = system::get_contract_args< chain::verify_block_signature_args >();
   auto signature_data = pack::from_variable_blob< pow_signature_data >( args.signature_data );
   auto to_hash = pack::to_variable_blob( signature_data.nonce );
   to_hash.insert( to_hash.end(), args.digest.digest.begin(), args.digest.digest.end() );

   auto pow = pack::from_variable_blob< uint256_t >( system::hash( CRYPTO_SHA2_256_ID, to_hash ).digest );

   // Get/update difficulty from database
   auto target = get_and_update_difficulty( system::get_head_block_time() );

   if ( pow > target )
   {
      system::print( "pow did not meet target\n" );
      system::set_contract_return( false );
      system::exit_contract( 0 );
   }

   // Recover address from signature
   auto producer = system::recover_public_key( pack::to_variable_blob( signature_data.recoverable_signature ), args.digest );

   // Mint block reward to address
   auto koin_token = koinos::token( KOIN_CONTRACT );

   auto success = koin_token.mint( producer, BLOCK_REWARD );
   if ( !success )
   {
      system::print( "could not mint KOIN to producer address " + std::string( producer.data(), producer.size() ) + '\n' );
   }

   system::set_contract_return( success );
   system::exit_contract( 0 );

   return 0;
}
