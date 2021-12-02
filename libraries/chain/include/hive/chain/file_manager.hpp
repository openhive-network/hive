#pragma once

#include <hive/chain/index_file.hpp>
#include <hive/chain/block_log_file.hpp>

#include <hive/protocol/block.hpp>

#include <hive/chain/storage_description.hpp>

#include <boost/make_shared.hpp>

namespace hive { namespace chain {

  class file_manager
  {
    private:

      static bool idx_cmp( const block_log_index& a, const block_log_index& b );

    public:

      using index_collection = std::set<block_log_index>;

    private:

      block_log_file block_log;

      index_collection idxs;

      bool get_resume() const;
      uint64_t get_index_pos() const;

    public:

      file_manager();
      ~file_manager();

      block_log_file& get_block_log_file();

      const block_log_index& get_block_log_idx() const;
      const block_log_index& get_hash_idx() const;

      void construct_index();
  };

} } // hive::chain
