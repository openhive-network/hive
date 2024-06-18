#pragma once

#include <hive/chain/full_block.hpp>

#include <memory>

namespace hive { namespace chain {

  /**
   * @brief Allows lib storage independent from block log file.
   */
  class last_irreversible_block_access_i
  {
  public:
    virtual ~last_irreversible_block_access_i() = default;

    virtual std::shared_ptr<full_block_type> get_last_irreversible_block_data() const = 0;
    virtual void set_last_irreversible_block_data(
      const std::shared_ptr<full_block_type> full_block) = 0;
  };

} }
