#pragma once

#include <hive/protocol/block.hpp>

#include <fc/filesystem.hpp>

#include <boost/make_shared.hpp>

namespace hive { namespace chain {

  using namespace hive::protocol;

  struct storage_description
  {
    int       file_descriptor = -1;
    fc::path  file;
  };

  struct storage_description_ex : public storage_description
  {
    // only accessed when appending a block, doesn't need locking
    ssize_t block_log_size = 0;
  };

  class block_log_index
  {
    storage_description storage;

    private:

      void construct_index( const boost::shared_ptr<signed_block>& head_block, const storage_description_ex& desc, bool resume = false, uint64_t index_pos = 0 );

    public:

      block_log_index();
      ~block_log_index();

      ssize_t open( const fc::path& file );
      void prepare( const ssize_t index_size, const boost::shared_ptr<signed_block>& head_block, const storage_description_ex& desc );
      bool close();

      void non_empty_idx_info();
      void append( const void* buf, size_t nbyte );
      void read( uint32_t block_num, uint64_t& offset, uint64_t& size );
      vector<signed_block> read_block_range( uint32_t first_block_num, uint32_t count, int block_log_fd, const boost::shared_ptr<signed_block>& head_block );
  };

} } // hive::chain
