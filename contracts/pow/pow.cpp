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
   uint256_t      difficulty_target = 0;
   timestamp_type last_block_time = timestamp_type( 0 );
   timestamp_type block_window_time = timestamp_type( 0 );
   uint32_t       averaging_window = 0;
};

KOINOS_REFLECT( difficulty_metadata_v1,
   (difficulty_target)
   (last_block_time)
   (block_window_time)
   (averaging_window)
)

struct difficulty_metadata_v2
{
   uint256_t      difficulty_target = 0;
   timestamp_type last_block_time = timestamp_type( 0 );
   uint64_t       required_hc = 0;
   uint128_t      hc_reserve;
   uint128_t      us_reserve;
   timestamp_type target_block_interval = timestamp_type( TARGET_BLOCK_INTERVAL_MS / 1000 );
};

KOINOS_REFLECT( difficulty_metadata_v2,
   (difficulty_target)
   (last_block_time)
   (required_hc)
   (hc_reserve)
   (us_reserve)
   (target_block_interval)
)

uint64_t get_hc_reserve_multiplier(uint32_t effective_us)
{
   // Rescale so that 2^32 represents one half-life
   uint64_t udt = (uint64_t( effective_us ) << 32) / (uint64_t(1000) * HC_HALFLIFE_MS);
   int64_t idt = int64_t(udt);

   int64_t y = -0xa2b23f3;
   y *= idt;
   y >>= 32;
   y += 0x3b9d3bec;
   y *= idt;
   y >>= 32;
   y -= 0xb17217f7;
   y *= idt;

   y >>= 32;
   y += 0x100000000;
   if( y < 0 )
      y = 0;
   return uint64_t( y );
}

void set_difficulty_target( difficulty_metadata_v2& diff_meta )
{
   if( diff_meta.us_reserve <= TARGET_BLOCK_INTERVAL_MS )
   {
      diff_meta.difficulty_target = 0;
      return;
   }

   // TODO:  Overflow analysis (#20)
   diff_meta.required_hc = uint64_t( ((1000 * TARGET_BLOCK_INTERVAL_MS) * diff_meta.hc_reserve) / (diff_meta.us_reserve - (1000 * TARGET_BLOCK_INTERVAL_MS)) );
   ++diff_meta.required_hc;
   diff_meta.difficulty_target = std::numeric_limits< uint256_t >::max() / diff_meta.required_hc;
}

difficulty_metadata_v2 initialize_difficulty( const difficulty_metadata_v1& pow1_meta )
{
   // TODO: Overflow analysis (#20)
   uint64_t initial_hashes_per_block = ( std::numeric_limits< uint256_t >::max() / pow1_meta.difficulty_target ).convert_to< uint64_t >();
   uint64_t initial_hashes_per_second = ( initial_hashes_per_block * 1000 ) / TARGET_BLOCK_INTERVAL_MS;
   uint64_t effective_us = uint64_t( 1000 ) * TARGET_BLOCK_INTERVAL_MS;
   uint64_t reserve_multiplier = get_hc_reserve_multiplier(effective_us);
   uint64_t sub_fraction = (uint64_t(1) << 32) - reserve_multiplier;

   difficulty_metadata_v2 diff_meta;

   // Compute initial hc_reserve by assuming dt's of effective_us have no net effect:
   //
   // + initial_hashes_per_block - sub_fraction*x = 0
   // x = initial_hashes_per_block / sub_fraction
   diff_meta.hc_reserve = (uint128_t(initial_hashes_per_block) << 32) / sub_fraction;

   // Compute initial us_reserve by assuming hc_reserve / (us_reserve / 1000000) = initial_hashes_per_secnod
   // -> hc_reserve / initial_hashes_per_block = us_reserve / 1000000
   // -> us_reserve = 1000000 * hc_reserve / initial_hashes_per_block
   diff_meta.us_reserve = (diff_meta.hc_reserve * 1000 ) / initial_hashes_per_second;

   set_difficulty_target(diff_meta);

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

/**
 * Modulate input time delta by diminishing returns penalty / bonus factors.
 *
 * Result is scaled so that a value of 1,000,000 corresponds to one second.
 */
uint64_t get_effective_us(uint64_t dt_ms)
{
   // Given diminishing returns function f(x) = x / (1+x),
   // compute f(t/k).  The result is f(t/k) = (t/k) / (1+t/k),
   // but we can multiply top and bottom by k to get
   // t / (k + t).

   // TODO:  Overflow analysis of this function (#20)
   uint128_t dt_ms_prod = uint128_t(dt_ms) * DR_BONUS;

   // The numerator needs to be multiplied by 2^32 * r,
   // which is exactly the value of the bonus.
   // The denominator needs to be multiplied by the bonus only, so it is shifted.
   uint128_t num = dt_ms_prod;
   num *= uint64_t(1000)*FREE_RUN_SAT_MULT*TARGET_BLOCK_INTERVAL_MS;
   uint128_t denom = dt_ms;
   denom += FREE_RUN_SAT_MULT*TARGET_BLOCK_INTERVAL_MS;
   denom <<= 32;

   return uint64_t(num / denom);
}

void update_difficulty( difficulty_metadata_v2& diff_meta, timestamp_type current_block_time )
{
   uint64_t dt = uint64_t(current_block_time) - uint64_t(diff_meta.last_block_time);

   // The order of the following computations assumes that the presented work
   // is checked against the difficulty target before this function is called.

   // TODO:  Overflow analysis of these computations (#20)
   // Add the MM transactions representing the delivery of a block.
   diff_meta.hc_reserve += diff_meta.required_hc;
   diff_meta.us_reserve -= 1000 * TARGET_BLOCK_INTERVAL_MS;

   // Free-running from the last block to the current block.

   uint64_t effective_us = get_effective_us( uint64_t(dt) );
   uint64_t reserve_multiplier = get_hc_reserve_multiplier(effective_us);

   diff_meta.us_reserve += effective_us;
   diff_meta.hc_reserve = (diff_meta.hc_reserve * reserve_multiplier) >> 32;

   // Setup the difficulty_target and required_hc
   set_difficulty_target(diff_meta);

   diff_meta.last_block_time = current_block_time;

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

   if ( pow > diff_meta.difficulty_target )
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
