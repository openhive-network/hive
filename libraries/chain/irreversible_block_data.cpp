#include <hive/chain/irreversible_block_data.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/full_block.hpp>

namespace hive { namespace chain {

block_data_type& block_data_type::operator=( const full_block_type& new_block )
{
  const compressed_block_data& new_cbd = new_block.get_compressed_block();
  size_t new_byte_size = new_cbd.compressed_size;
  const char* new_bytes = new_cbd.compressed_bytes.get();

  _compression_attributes = new_cbd.compression_attributes;
  _byte_size = new_byte_size;
  _block_bytes.assign( new_bytes, new_bytes+new_byte_size );
  _block_id = new_block.get_block_id();

  return *this;
}

std::shared_ptr<full_block_type> block_data_type::create_full_block() const
{
  if( protocol::block_header::num_from_id( _block_id ) == 0 )
    return std::shared_ptr<full_block_type>();

  try
  {
    FC_ASSERT( _byte_size > 0 && "Data mismatch in last_irreversible_block_data!" );

    std::unique_ptr<char[]> compressed_block_data( new char[_byte_size] );
    memcpy( compressed_block_data.get(), _block_bytes.data(), _byte_size );

    return full_block_type::create_from_compressed_block_data(
      std::move( compressed_block_data ),
      _byte_size,
      _compression_attributes,
      std::optional< block_id_type >( _block_id )
    );
  }
  FC_CAPTURE_AND_RETHROW()
}

} } //hive::chain
