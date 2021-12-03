#pragma once

#include <hive/protocol/block.hpp>

#include <hive/chain/storage_description.hpp>

#include <fc/filesystem.hpp>

#include <boost/make_shared.hpp>

namespace hive { namespace chain {

  using namespace hive::protocol;

  class block_log_index
  {
    private:

      const uint32_t ELEMENT_SIZE = 8;

      void read_blocks_number( uint64_t block_pos );

    public:

      storage_description storage;

      block_log_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~block_log_index();

      void check_consistency( uint32_t total_size );
      void open( const fc::path& file );
      void open();
      void prepare( const boost::shared_ptr<signed_block>& head_block, const storage_description& desc );
      void close();

      void non_empty_idx_info();
      void append( const void* buf, size_t nbyte );
      void read( uint32_t block_num, uint64_t& offset, uint64_t& size );
      vector<signed_block> read_block_range( uint32_t first_block_num, uint32_t count, int block_log_fd, const boost::shared_ptr<signed_block>& head_block );
  };

} } // hive::chain
