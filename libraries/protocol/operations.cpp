#include <hive/protocol/operations.hpp>

#include <hive/protocol/operation_util_impl.hpp>

namespace hive { namespace protocol {

struct is_market_op_visitor {
  typedef bool result_type;

  template<typename T>
  bool operator()( T&& v )const { return false; }
  bool operator()( const limit_order_create_operation& )const { return true; }
  bool operator()( const limit_order_cancel_operation& )const { return true; }
  bool operator()( const transfer_operation& )const { return true; }
  bool operator()( const transfer_to_vesting_operation& )const { return true; }
};

bool is_market_operation( const operation& op ) {
  return op.visit( is_market_op_visitor() );
}

struct is_vop_visitor
{
  typedef bool result_type;

  template< typename T >
  bool operator()( const T& v )const { return v.is_virtual(); }
};

bool is_virtual_operation( const operation& op )
{
  return op.visit( is_vop_visitor() );
}

struct is_effective_op_visitor
{
  typedef bool result_type;

  template<typename T>
  bool operator()( T&& v )const { return true; }
  bool operator()( const fill_vesting_withdraw_operation& op )const { return !op.is_empty_implied_route(); }
};

bool is_effective_operation( const operation& op )
{
  return op.visit( is_effective_op_visitor() );
}

} } // hive::protocol

HIVE_DEFINE_OPERATION_TYPE( hive::protocol::operation )
