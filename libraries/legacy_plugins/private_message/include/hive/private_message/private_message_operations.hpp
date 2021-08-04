
#pragma once

#include <hive/protocol/base.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/operation_util.hpp>

#include <fc/reflect/reflect.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace hive { namespace private_message {

struct private_message_operation : public hive::protocol::base_operation
{
  protocol::account_name_type  from;
  protocol::account_name_type  to;
  protocol::public_key_type    from_memo_key;
  protocol::public_key_type    to_memo_key;
  uint64_t                     sent_time = 0; /// used as seed to secret generation
  uint32_t                     checksum = 0;
  std::vector<char>            encrypted_message;
};

typedef fc::static_variant< private_message_operation > private_message_plugin_operation;

} }

namespace fc
{
  using hive::private_message::private_message_plugin_operation;
  template<>
  struct serialization_functor< private_message_plugin_operation >
  {
    bool operator()( const fc::variant& v, private_message_plugin_operation& s ) const
    {
      return extended_serialization_functor< private_message_plugin_operation >().serialize( v, s );
    }
  };
}

FC_REFLECT( hive::private_message::private_message_operation, (from)(to)(from_memo_key)(to_memo_key)(sent_time)(checksum)(encrypted_message) )

HIVE_DECLARE_OPERATION_TYPE( hive::private_message::private_message_plugin_operation )
FC_REFLECT_TYPENAME( hive::private_message::private_message_plugin_operation )
