#pragma once

#include <hive/chain/block_data.hpp>
#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace chain {
  
class abstract_block_producer
{
public:
  virtual ~abstract_block_producer() = default;

  virtual void generate_block( hive::chain::new_block_data* request ) = 0;
};

} } } // hive::plugins::chain
