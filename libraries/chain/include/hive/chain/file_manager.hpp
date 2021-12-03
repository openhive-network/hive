#pragma once

#include <hive/chain/index_file.hpp>
#include <hive/chain/block_log_file.hpp>

#include <hive/protocol/block.hpp>

#include <hive/chain/storage_description.hpp>

#include <boost/make_shared.hpp>

namespace hive { namespace chain {

  class file_manager
  {
    public:

      using p_base_index      = std::shared_ptr<base_index>;
      using index_collection  = std::vector<p_base_index>;

    private:

      //at now there are only 2 indexes
      enum { BLOCK_LOG_IDX = 0, HASH_IDX };

      block_log_file block_log;

      index_collection idxs;

      bool get_resume() const;
      uint64_t get_index_pos() const;

    public:

      file_manager();
      ~file_manager();

      void close();

      block_log_file& get_block_log_file();

      p_base_index& get_block_log_idx();
      p_base_index& get_hash_idx();

      void construct_index();
  };

} } // hive::chain
