#pragma once

#include <steem/protocol/block.hpp>

namespace hive { namespace chain {

struct transaction_notification
{
   transaction_notification( const hive::protocol::signed_transaction& tx ) : transaction(tx)
   {
      transaction_id = tx.id();
   }

   hive::protocol::transaction_id_type          transaction_id;
   const hive::protocol::signed_transaction&    transaction;
};

} }
