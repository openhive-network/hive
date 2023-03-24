#pragma once

#include <boost/container/flat_set.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/forward_impacted.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

  /**
   * @brief After visiting, returns map of accounts and their required minimum balances to successfully evaluate
   */
  class ops_impacted_accounts_visitor
  {
  private:
    boost::container::flat_set<hive::protocol::account_name_type>& accounts_new;
    boost::container::flat_set<hive::protocol::account_name_type>& accounts_created;
    blockchain_converter& converter;

  public:
    typedef void result_type;

    ops_impacted_accounts_visitor(
      boost::container::flat_set<account_name_type>& accounts_new,
      boost::container::flat_set<account_name_type>& accounts_created,
      blockchain_converter& converter
    )
      : accounts_new( accounts_new ), accounts_created( accounts_created ), converter( converter )
    {}

    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );

      hive::app::operation_get_impacted_accounts(op, accounts_new);
    }

    // Operations creating accounts that should not collect accounts to be created:

    // TODO: Handle claim_account_operation and create_claimed_account_operation

    result_type operator()( const hive::protocol::account_create_operation& op )const
    {
      accounts_new.insert( op.creator );
      accounts_created.insert( op.new_account_name );
    }
    result_type operator()( const hive::protocol::pow_operation& op )const
    {
      accounts_created.insert( op.worker_account );
    }
    result_type operator()( const hive::protocol::account_create_with_delegation_operation& op )const
    {
      accounts_new.insert( op.creator );
      accounts_created.insert( op.new_account_name );
    }
    result_type operator()( hive::protocol::pow2_operation& op )const {
      const auto& input = op.work.which() ? op.work.get< hive::protocol::equihash_pow >().input : op.work.get< hive::protocol::pow2 >().input;

      if( accounts_created.find( input.worker_account ) == accounts_created.end() )
      {
        if( !op.new_owner_key.valid() )
          op.new_owner_key = converter.get_witness_key().get_public_key();

        accounts_created.insert( input.worker_account );
      }
    }
  };

} } } } }
