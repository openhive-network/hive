#pragma once

#include <hive/protocol/block.hpp>

namespace hive { namespace chain {

struct transaction_notification
{
  transaction_notification( const hive::protocol::signed_transaction& tx, const fc::raw::pack_flags& flags ) : transaction(tx)
  {
    transaction_id = tx.id( flags );
  }

  hive::protocol::transaction_id_type          transaction_id;
  const hive::protocol::signed_transaction&    transaction;
};

} }
