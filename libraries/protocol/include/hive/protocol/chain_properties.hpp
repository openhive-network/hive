#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace protocol {

  /**
    * Witnesses must vote on how to set certain chain properties to ensure a smooth
    * and well functioning network.  Any time @owner is in the active set of witnesses these
    * properties will be used to control the blockchain configuration.
    */
  struct chain_properties
  {
    /**
      *  This fee, paid in HIVE, is converted into VESTING SHARES for the new account. Accounts
      *  without vesting shares cannot earn usage rations and therefore are powerless. This minimum
      *  fee requires all accounts to have some kind of commitment to the network that includes the
      *  ability to vote and make transactions.
      */
    asset             account_creation_fee =
      asset( HIVE_MIN_ACCOUNT_CREATION_FEE, HIVE_SYMBOL );

    /**
      *  This witnesses vote for the maximum_block_size which is used by the network
      *  to tune rate limiting and capacity
      */
    uint32_t          maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
    uint16_t          hbd_interest_rate  = HIVE_DEFAULT_HBD_INTEREST_RATE;
    /**
      * How many free accounts should be created per elected witness block.
      * Scaled so that HIVE_ACCOUNT_SUBSIDY_PRECISION represents one account.
      */
    int32_t           account_subsidy_budget = HIVE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET;

    /**
      * What fraction of the "stockpiled" free accounts "expire" per elected witness block.
      * Scaled so that 1 << HIVE_RD_DECAY_DENOM_SHIFT represents 100% of accounts
      * expiring.
      */
    uint32_t          account_subsidy_decay = HIVE_DEFAULT_ACCOUNT_SUBSIDY_DECAY;
  };


} } // hive::protocol

FC_REFLECT( hive::protocol::chain_properties,
          (account_creation_fee)
          (maximum_block_size)
          (hbd_interest_rate)
          (account_subsidy_budget)
          (account_subsidy_decay)
        )
