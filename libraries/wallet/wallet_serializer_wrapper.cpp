#include <hive/protocol/misc_utilities.hpp>
#include <hive/wallet/wallet_serializer_wrapper.hpp>

namespace hive { namespace wallet {

  hive::protocol::transaction_serialization_type wallet_transaction_serialization::transaction_serialization = hive::protocol::serialization_mode_controller::default_transaction_serialization;

} } //hive::wallet
