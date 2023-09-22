#pragma once

#include <hive/chain/block_log_reader.hpp>

#include <hive/chain/fork_database.hpp>

namespace hive { namespace chain {

  class fork_db_block_reader : public block_log_reader
  {
  public:
    fork_db_block_reader( const fork_database& fork_db, block_log& block_log );
    virtual ~fork_db_block_reader() = default;

    virtual bool is_known_block(const block_id_type& id) const override;

  private:
    const fork_database& _fork_db;
  };

} }
