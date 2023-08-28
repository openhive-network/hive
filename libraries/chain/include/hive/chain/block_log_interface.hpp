#pragma once

#include <hive/protocol/block.hpp>

namespace hive { namespace chain {

  class full_block_type;

  class block_log_read_i
  {
  public:
    // block_log::read_block_by_num
    // postgres_block_log::get_full_block
    virtual std::shared_ptr<full_block_type> get_full_block(uint32_t block_num) const = 0;
    
    // block_log::read_block_range_by_num
    // postgres_block_log::get_postgres_data
    virtual std::vector<std::shared_ptr<full_block_type>> read_block_range_by_num(uint32_t first_block_num, uint32_t count) const = 0;
    
    // block_log::read_block_id_by_num
    // postgres_block_log:: to be implemented
    virtual hive::protocol::block_id_type read_block_id_by_num(uint32_t block_num) const = 0;
  };

} }

