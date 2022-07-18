#pragma once

#include <hive/chain/block_flow_control.hpp>
#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace chain {
  
class abstract_block_producer
{
public:
  virtual ~abstract_block_producer() = default;

  virtual void generate_block( hive::chain::new_block_flow_control* new_block_ctrl ) = 0;
};

} } } // hive::plugins::chain
