#pragma once

#include <map>
#include <vector>

#include <hive/protocol/asset.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

  /**
   * @brief After visiting, returns map of accounts and their required minimum balances to successfully evaluate
   */
  class ops_required_asset_transfer_visitor
  {
  private:
    uint32_t hf;

  public:
    ops_required_asset_transfer_visitor( uint32_t hf )
      : hf( hf )
    {}

    typedef std::map<hive::protocol::account_name_type, std::vector<hive::protocol::asset>> result_type;

    result_type operator()( hive::protocol::create_proposal_operation& o )const
    {
      using hive::protocol::asset;

      asset fee_hbd( HIVE_TREASURY_FEE, HBD_SYMBOL );

      if( hf >= HIVE_HARDFORK_1_24 )
      {
        uint32_t proposal_run_time = o.end_date.sec_since_epoch() - o.start_date.sec_since_epoch();

        if(proposal_run_time > HIVE_PROPOSAL_FEE_INCREASE_DAYS_SEC)
        {
          uint32_t extra_days = (proposal_run_time / HIVE_ONE_DAY_SECONDS) - HIVE_PROPOSAL_FEE_INCREASE_DAYS;
          fee_hbd += asset(HIVE_PROPOSAL_FEE_INCREASE_AMOUNT * extra_days, HBD_SYMBOL);
        }
      }

      return { { o.creator, { fee_hbd } } };
    }

    result_type operator()( hive::protocol::account_create_operation& o )const
    {
      return { { o.creator, { o.fee } } };
    }

    result_type operator()( hive::protocol::account_create_with_delegation_operation& o )const
    {
      return { { o.creator, { o.fee, o.delegation } } };
    }

    result_type operator()( hive::protocol::transfer_operation& o )const
    {
      return { { o.from, { o.amount } } };
    }

    result_type operator()( hive::protocol::transfer_to_vesting_operation& o )const
    {
      return { { o.from, { o.amount } } };
    }

    result_type operator()( hive::protocol::convert_operation& o )const
    {
      return { { o.owner, { o.amount } } };
    }

    result_type operator()( hive::protocol::collateralized_convert_operation& o )const
    {
      return { { o.owner, { o.amount } } };
    }

    result_type operator()( hive::protocol::limit_order_create_operation& o )const
    {
      return { { o.owner, { o.amount_to_sell } } };
    }

    result_type operator()( hive::protocol::limit_order_create2_operation& o )const
    {
      return { { o.owner, { o.amount_to_sell } } };
    }

    result_type operator()( hive::protocol::transfer_to_savings_operation& o )const
    {
      return { { o.from, { o.amount } } };
    }

    // No signatures modification ops
    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return {};
    }
  };

} } } } }
