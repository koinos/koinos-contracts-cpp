#include <koinos/system/system_calls.hpp>
#include <koinos/token.hpp>

#include <koinos/contracts/pow/pow.h>

#include <boost/multiprecision/cpp_int.hpp>

#include <sstream>

using namespace koinos;

using uint256_t = boost::multiprecision::uint256_t;
using EmbeddedProto::FieldBytes;

enum class entries : uint32_t
{
   get_difficulty = 0x4a758831,
};

namespace constants {

constexpr std::size_t max_buffer_size         = 2048;
constexpr std::size_t max_signature_size      = 65;
const std::string contract_space              = system::get_contract_id();
const std::string difficulty_metadata_key     = "";
constexpr std::size_t target_block_interval_s = 10;
constexpr uint64_t sha256_id                  = 0x12;
constexpr uint64_t pow_end_date               = 1640995199000; // 2021-12-31T23:59:59Z
constexpr uint64_t block_reward               = 10000000000;
const std::string koin_contract               = "0x930f7a991f8787bd485b02684ffda52570439794";

} // constants

using pow_signature_data = koinos::contracts::pow::pow_signature_data< 32, 65 >;
using difficulty_metadata = koinos::contracts::pow::difficulty_metadata< 32, 32 >;
using verify_block_signature_args
   = koinos::chain::verify_block_signature_args<
      koinos::system::detail::max_hash_size,
      koinos::system::detail::max_active_data_size,
      koinos::system::detail::max_hash_size >;

using active_block_data
   = koinos::protocol::active_block_data<
      koinos::system::detail::max_hash_size,
      koinos::system::detail::max_hash_size,
      constants::max_signature_size >;


template< uint32_t MAX_LENGTH >
void to_binary( FieldBytes< MAX_LENGTH >& f, const uint256_t& n )
{
   static_assert( MAX_LENGTH >= 32 );
   auto num = n;

   // This is not optimal, consider using boost::mutliprecison::export_bits
   for ( size_t i = 0; i < 32; i++ )
   {
      f[32 - i] = ( num % 256 ).convert_to< char >();
      num >>= 8;
   }
}

template< uint32_t MAX_LENGTH >
void from_binary( const FieldBytes< MAX_LENGTH >& f, uint256_t& n, size_t start = 0 )
{
   assert( MAX_LENGTH >= start + 32 );

   for ( size_t i = start; i < start + 32; i++ )
   {
      n <<= 8;
      n += f[i];
   }
}

std::string to_hex( const std::string& s )
{
   std::stringstream stream;
   stream << "0x" << std::hex << std::setfill( '0' );
   for ( const auto& c : s )
   {
      stream << std::setw( 2 ) << static_cast< unsigned int >( static_cast< unsigned char >( c ) );
   }
   return stream.str();
}


void initialize_difficulty( difficulty_metadata diff_meta )
{
   uint256_t target = std::numeric_limits< uint256_t >::max() / (1 << 20);
   auto difficulty = 1 << 20;
   to_binary( diff_meta.mutable_target(), target );
   diff_meta.set_last_block_time( system::get_head_info().get_head_block_time() );
   to_binary( diff_meta.mutable_difficulty(), difficulty );
   diff_meta.set_target_block_interval( constants::target_block_interval_s );
}

difficulty_metadata get_difficulty_meta()
{
   difficulty_metadata diff_meta;
   if ( !system::get_object( constants::contract_space, constants::difficulty_metadata_key, diff_meta ) )
   {
      initialize_difficulty( diff_meta );
   }

   return diff_meta;
}

void update_difficulty( difficulty_metadata& diff_meta, uint64_t current_block_time )
{
   uint256_t difficulty;
   from_binary( diff_meta.get_difficulty(), difficulty );
   difficulty = difficulty + difficulty / 2048 * std::max(1 - int64_t((current_block_time - diff_meta.last_block_time()) / 7000), -99ll);
   to_binary( diff_meta.mutable_difficulty(), difficulty );
   diff_meta.set_last_block_time( current_block_time );
   auto target = std::numeric_limits< uint256_t >::max() / difficulty;
   to_binary( diff_meta.mutable_target(), target );

   system::put_object( constants::contract_space, constants::difficulty_metadata_key, diff_meta );
}

int main()
{
   auto entry_point = system::get_entry_point();

   std::array< uint8_t, constants::max_buffer_size > retbuf;
   koinos::write_buffer buffer( retbuf.data(), retbuf.size() );

   if ( entry_point == std::underlying_type_t< entries >( entries::get_difficulty ) )
   {
      get_difficulty_meta().serialize( buffer );
      std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
      system::set_contract_return_bytes( retval );
      system::exit_contract( 0 );
   }

   koinos::chain::verify_block_signature_return ret;
   ret.mutable_value() = false;

   auto head_block_time = system::get_head_info().get_head_block_time();
   if ( uint64_t( head_block_time ) > constants::pow_end_date )
   {
      system::print( "testnet has ended" );
      ret.serialize( buffer );
      std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
      system::set_contract_return_bytes( retval );
      system::exit_contract( 0 );
   }

   const auto [ caller, privilege ] = system::get_caller();
   if ( privilege != chain::privilege::kernel_mode )
   {
      system::print( "pow contract must be called from kernel" );\
      ret.serialize( buffer );
      std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
      system::set_contract_return_bytes( retval );
      system::exit_contract( 0 );
   }

   auto argstr = system::get_contract_args();
   koinos::read_buffer rdbuf( (uint8_t*)argstr.c_str(), argstr.size() );
   verify_block_signature_args args;
   args.deserialize( rdbuf );

   pow_signature_data sig_data;
   rdbuf = koinos::read_buffer( const_cast< uint8_t* >( reinterpret_cast< const uint8_t* >( args.get_signature_data().get_const() ) ), args.get_signature_data().get_length());
   sig_data.deserialize( rdbuf );

   std::string nonce_str( reinterpret_cast< const char* >( sig_data.get_nonce().get_const() ), sig_data.get_nonce().get_length() );
   std::string digest_str( const_cast< char* >( reinterpret_cast< const char* >( args.get_digest().get_const() ) ) + 2, args.get_digest().get_length() - 2 );
   nonce_str.insert( nonce_str.end(), digest_str.begin(), digest_str.end() );

   auto pow = system::hash( constants::sha256_id, nonce_str );

   // Get/update difficulty from database
   auto diff_meta = get_difficulty_meta();

   if ( memcmp( pow.c_str() + 2, diff_meta.get_target().get_const(), pow.size() - 2 ) > 0 )
   {
      system::print( "pow did not meet target\n" );
      ret.serialize( buffer );
      std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
      system::set_contract_return_bytes( retval );
      system::exit_contract( 0 );
   }

   update_difficulty( diff_meta, head_block_time );

   // Recover address from signature
   std::string sig_str( reinterpret_cast< const char* >( sig_data.get_recoverable_signature().get_const() ), sig_data.get_recoverable_signature().get_length() );
   auto producer = system::recover_public_key( sig_str, digest_str );

   active_block_data active;
   rdbuf = koinos::read_buffer( const_cast< uint8_t* >( reinterpret_cast< const uint8_t* >( args.get_active().get_const() ) ), args.get_active().get_length() );
   active.deserialize( rdbuf );

   std::string signer( reinterpret_cast< const char* >( active.get_signer().get_const() ), active.get_signer().get_length() );

   if ( producer != signer )
   {
      system::print( "signature and signer are mismatching\n" );
      ret.serialize( buffer );
      std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
      system::set_contract_return_bytes( retval );
      system::exit_contract( 0 );
   }

   // Mint block reward to address
   auto koin_token = koinos::token( constants::koin_contract );

   auto success = koin_token.mint( signer, constants::block_reward );
   if ( !success )
   {
      system::print( "could not mint KOIN to producer address " + to_hex( producer ) + '\n' );
   }

   ret.set_value( false );

   std::string retval( reinterpret_cast< const char* >( retbuf.data() ), retbuf.size() );
   ret.serialize( buffer );
   system::set_contract_return_bytes( retval );
   system::exit_contract( 0 );

   return 0;
}
