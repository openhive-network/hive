#include <hive/chain/full_block_object.hpp>

namespace hive { namespace chain {

std::shared_ptr<full_block_type> full_block_object::create_full_block() const
{
  try
  {
    FC_ASSERT( byte_size > 0 && "No data in full block object!" );

    size_t raw_block_size = this->byte_size;
    std::unique_ptr<char[]> compressed_block_data(new char[raw_block_size]);
    memcpy(compressed_block_data.get(), this->block_bytes.data(), raw_block_size);
    const block_attributes_t& attributes = this->compression_attributes;
    std::optional< block_id_type > block_id( this->block_id );

    return full_block_type::create_from_compressed_block_data(
      std::move( compressed_block_data ),
      raw_block_size,
      attributes,
      block_id
    );
  }
  FC_CAPTURE_AND_RETHROW()
}

} } // hive::chain
