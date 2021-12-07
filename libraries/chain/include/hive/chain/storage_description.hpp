#pragma once

#include <fc/filesystem.hpp>

namespace hive { namespace chain {

  struct storage_description
  {
    enum status_type  : uint32_t { none, reopen, resume };
    enum storage_type : uint32_t { block_log, block_log_idx, hash_idx };

    status_type status = status_type::none;
    const storage_type storage;

    // only accessed when appending a block, doesn't need locking
    ssize_t   size  = 0;
    uint64_t  pos   = 0;

    int       file_descriptor = -1;

    std::string file_name_ext;
    fc::path    file;

    storage_description( storage_type val, const std::string& file_name_ext_val = "" );

    void calculate_status( uint64_t block_pos );
    void check_consistency( uint32_t element_size, uint32_t total_size );
    void open( const fc::path& input_file );
    void open();
    void close();

    bool is_open() const;
  };

} } // hive::chain
