
#pragma once

#include <hive/protocol/operation_util.hpp>

#include <fc/static_variant.hpp>

namespace hive { namespace protocol {

struct operation_validate_visitor
{
  typedef void result_type;
  template<typename T>
  void operator()( const T& v )const { v.validate(); }
};

} } // hive::protocol

//
// Place HIVE_DEFINE_OPERATION_TYPE in a .cpp file to define
// functions related to your operation type
//
#define HIVE_DEFINE_OPERATION_TYPE( OperationType )                       \
                                                  \
namespace hive { namespace protocol {                                     \
                                                  \
void operation_validate( const OperationType& op )                         \
{                                                                          \
  op.visit( hive::protocol::operation_validate_visitor() );              \
}                                                                          \
                                                  \
void operation_get_required_authorities( const OperationType& op,          \
                            flat_set< account_name_type >& active,         \
                            flat_set< account_name_type >& owner,          \
                            flat_set< account_name_type >& posting,        \
                            flat_set< account_name_type >& witness,        \
                            std::vector< authority >& other ) \
{                                                                          \
  op.visit( hive::protocol::get_required_auth_visitor( active, owner, posting, witness, other ) ); \
}                                                                          \
                                                  \
} } /* hive::protocol */
