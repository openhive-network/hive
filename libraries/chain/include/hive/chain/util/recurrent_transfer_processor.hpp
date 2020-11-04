#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive { namespace chain {

  using namespace protocol;
  struct recurrent_transfer_data
  {
    time_point_sec time;
    account_name_type from;
    account_name_type to;
    asset             amount;
    /// The memo is plain-text, any encryption on the memo is up to a higher level protocol.
    string            memo;
  };

} } // namespace hive::chain

FC_REFLECT( hive::chain::recurrent_transfer_data,
            (time)(from)(to)(amount)(memo)
)
