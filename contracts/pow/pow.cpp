#include <koinos/system/system_calls.hpp>
#include <koinos/token.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using namespace koinos;

#define KOIN_CONTRACT uint160_t( "0x930f7a991f8787bd485b02684ffda52570439794" )
#define BLOCK_REWARD  10000000000 // 100 KOIN
#define TARGET_BLOCK_INTERVAL_S 10
#define DIFFICULTY_METADATA_KEY uint256_t( 0 )
#define GET_DIFFICULTY_ENTRYPOINT 0x4a758831
#define CRYPTO_SHA2_256_ID uint64_t(0x12)
#define POW_END_DATE 1640995199000 // 2021-12-31T23:59:59Z

using uint256_t = boost::multiprecision::uint256_t;

uint256_t contract_id;

struct pow_signature_data
{
   uint256_t        nonce;
   fixed_blob< 65 > recoverable_signature;
};


struct difficulty_metadata
{
   uint256_t      target = 0;
   uint64_t       last_block_time = timestamp_type( 0 );
   uint256_t      difficulty = 0;
   uint64_t       target_block_interval = TARGET_BLOCK_INTERVAL_S;
};


void initialize_difficulty( difficulty_metadata diff_meta )
{
   diff_meta.target = std::numeric_limits< uint256_t >::max() / (1 << 20),
   diff_meta.last_block_time = system::get_head_block_time(),
   diff_meta.difficulty = 1 << 20,
   diff_meta.target_block_interval = TARGET_BLOCK_INTERVAL_S;
}

difficulty_metadata get_difficulty_meta()
{
   difficulty_metadata diff_meta;
   if ( !system::get_object( contract_id, DIFFICULTY_METADATA_KEY, diff_meta ) )
   {
      initialize_difficulty( diff_meta );
   }

   return diff_meta;
}

void update_difficulty( difficulty_metadata_v2& diff_meta, timestamp_type current_block_time )
{
   diff_meta.difficulty = diff_meta.difficulty + diff_meta.difficulty / 2048 * std::max(1 - int64((current_block_time - diff_meta.last_block_time) / 7000), -99ll);
   diff_meta.last_block_time = current_block_time;
   diff_meta.target = std::numeric_limits< uint256_t >::max() / diff_meta.difficulty;

   system::put_object( contract_id, DIFFICULTY_METADATA_KEY, diff_meta );
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

   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      system::print( "pow contract must be called from kernel" );
      system::set_contract_return( false );
      system::exit_contract( 0 );
   }

   auto args = system::get_contract_args();
   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );


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
