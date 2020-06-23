
#pragma once

#include <memory>

#include <hive/protocol/types.hpp>

namespace hive { namespace schema {
  struct abstract_schema;
} }

namespace hive { namespace protocol {
  struct custom_json_operation;
} }

namespace hive { namespace chain {

class custom_operation_interpreter
{
  public:
    virtual void apply( const protocol::custom_json_operation& op ) = 0;
    virtual void apply( const protocol::custom_binary_operation & op ) = 0;
    virtual hive::protocol::custom_id_type get_custom_id() = 0;
    virtual std::shared_ptr< hive::schema::abstract_schema > get_operation_schema() = 0;
};

} } // hive::chain
