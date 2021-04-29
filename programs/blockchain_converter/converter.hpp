#pragma once

#include <hive/protocol/operations.hpp>

namespace hive {

  using namespace protocol;

  namespace converter {

  struct convert_operations_visitor
  {
    typedef operation result_type;

    const account_create_operation& operator()( const account_create_operation& op )const
    { return op; }

    const account_create_with_delegation_operation& operator()( const account_create_with_delegation_operation& op )const
    { return op; }

    const account_update_operation& operator()( const account_update_operation& op )const
    { return op; }

    const account_update2_operation& operator()( const account_update2_operation& op )const
    { return op; }

    const create_claimed_account_operation& operator()( const create_claimed_account_operation& op )const
    { return op; }

    const witness_update_operation& operator()( const witness_update_operation& op )const
    { return op; }

    const witness_set_properties_operation& operator()( const witness_set_properties_operation& op )const
    { return op; }

    const custom_binary_operation& operator()( const custom_binary_operation& op )const
    { return op; }

    const pow2_operation& operator()( const pow2_operation& op )const
    { return op; }

    const report_over_production_operation& operator()( const report_over_production_operation& op )const
    { return op; }

    const request_account_recovery_operation& operator()( const request_account_recovery_operation& op )const
    { return op; }

    const recover_account_operation& operator()( const recover_account_operation& op )const
    { return op; }

    // No signatures modification ops
    template< typename T >
    const T& operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return op;
    }
  };

} }