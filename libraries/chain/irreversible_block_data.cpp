#include <hive/chain/irreversible_block_data.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/full_block.hpp>

#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

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

std::string get_irreversible_block_details(chainbase::database& db)
{
  try
  {
    auto segment_manager = db.get_segment_manager();
    const auto result = segment_manager->find<irreversible_block_data_type>("irreversible");

    if(!result.first)
    {
      fc::mutable_variant_object vo;
      vo("error", "Irreversible block data not found in shared memory");
      return fc::json::to_pretty_string(fc::variant(vo));
    }

    const auto& irr = *result.first;
    const auto& block_data = irr._irreversible_block_data;

    fc::mutable_variant_object block_data_vo;
    block_data_vo("_compression_attributes", fc::variant(block_data._compression_attributes));
    block_data_vo("_byte_size", block_data._byte_size);
    // Note: _block_bytes is intentionally omitted — it uses boost::interprocess allocators
    // that cannot be serialized with fc::json, and its size is already represented by _byte_size.
    block_data_vo("_block_id", fc::variant(block_data._block_id));

    fc::mutable_variant_object vo;
    vo("_irreversible_block_num", irr._irreversible_block_num);
    vo("_irreversible_block_data", fc::variant(block_data_vo));

    return fc::json::to_pretty_string(fc::variant(vo));
  }
  catch(const fc::exception& e)
  {
    fc::mutable_variant_object vo;
    vo("error", "Failed to read irreversible block data: " + e.to_detail_string());
    return fc::json::to_pretty_string(fc::variant(vo));
  }
  catch(const std::exception& e)
  {
    fc::mutable_variant_object vo;
    vo("error", std::string("Failed to read irreversible block data: ") + e.what());
    return fc::json::to_pretty_string(fc::variant(vo));
  }
}

} } //hive::chain
