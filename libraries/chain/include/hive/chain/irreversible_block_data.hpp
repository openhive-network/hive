#pragma once

#include <chainbase/allocators.hpp>

#include <hive/chain/block_log_artifacts.hpp>

#include <hive/protocol/types.hpp>

#include <memory>

namespace hive {
namespace chain {

  class database;
  class full_block_type;
  using block_attributes_t = hive::chain::detail::block_attributes_t;
  using block_id_type = hive::protocol::block_id_type;
  using t_block_bytes = chainbase::t_vector< char >;

  /// Data set that a) allows to create full block type and b) can be stored in state memory file.
  struct block_data_type
  {
    block_attributes_t  _compression_attributes;
    size_t              _byte_size = 0;
    t_block_bytes       _block_bytes;
    block_id_type       _block_id;

    template< typename Allocator >
    block_data_type( chainbase::allocator< Allocator > a )
      : _block_bytes( a )
    {}

    block_data_type& operator=( const full_block_type& new_block );

    std::shared_ptr<full_block_type> create_full_block() const;
  };

  struct irreversible_block_data_type
  {
    uint32_t        _irreversible_block_num = 0;
    block_data_type _irreversible_block_data;

    template< typename Allocator >
    irreversible_block_data_type( chainbase::allocator< Allocator > a )
      : _irreversible_block_data( a )
    {}        
  };

} // namespace chain
} // namespace hive

FC_REFLECT(hive::chain::block_data_type,
  (_compression_attributes)(_byte_size)(_block_bytes)(_block_id))

FC_REFLECT(hive::chain::irreversible_block_data_type, 
  (_irreversible_block_num)(_irreversible_block_data))
