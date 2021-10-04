#pragma once

#include <fc/container/flat.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <fc/string.hpp>

namespace hive { namespace app {

using namespace fc;

void operation_get_impacted_accounts(
  const hive::protocol::operation& op,
  fc::flat_set<protocol::account_name_type>& result );

void transaction_get_impacted_accounts(
  const hive::protocol::transaction& tx,
  fc::flat_set<protocol::account_name_type>& result
  );

} } // hive::app
