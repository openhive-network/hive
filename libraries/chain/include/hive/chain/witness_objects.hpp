#pragma once
#include <fc/uint128.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/util/rd_dynamics.hpp>

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::digest_type;
  using hive::protocol::public_key_type;
  using hive::protocol::version;
  using hive::protocol::hardfork_version;
  using hive::protocol::price;
  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;
  using hive::protocol::VEST_asset;
  using hive::protocol::asset;
  using hive::protocol::asset_symbol_type;
  using hive::chain::util::rd_dynamics_params;

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

  /**
    *  All witnesses with at least 1% net positive approval and
    *  at least 2 weeks old are able to participate in block
    *  production.
    */
  class witness_object : public object< witness_object_type, witness_object >
  {
    CHAINBASE_OBJECT( witness_object );
    public:
      enum witness_schedule_type
      {
        elected,
        timeshare,
        miner,
        none
      };

      CHAINBASE_DEFAULT_CONSTRUCTOR( witness_object, (url) )

      //HBD to HIVE ratio proposed by the witness
      const price& get_hbd_exchange_rate() const { return hbd_exchange_rate; }
      //time when HBD/HIVE price ratio was last confirmed (TODO: add routine to check if price feed is valid)
      const time_point_sec& get_last_hbd_exchange_update() const { return last_hbd_exchange_update; }

      /** the account that has authority over this witness */
      account_name_type owner;
      time_point_sec    created;
      shared_string     url;
      uint32_t          total_missed = 0;
      uint64_t          last_aslot = 0;
      uint64_t          last_confirmed_block_num = 0;

      /**
        * Some witnesses have the job because they did a proof of work,
        * this field indicates where they were in the POW order. After
        * each round, the witness with the lowest pow_worker value greater
        * than 0 is removed.
        */
      uint64_t          pow_worker = 0;

      /**
        *  This is the key used to sign blocks on behalf of this witness
        */
      public_key_type   signing_key;

      chain_properties  props;
      price             hbd_exchange_rate;
      time_point_sec    last_hbd_exchange_update;


      /**
        *  The total votes for this witness. This determines how the witness is ranked for
        *  scheduling.  The top N witnesses by votes are scheduled every round, every one
        *  else takes turns being scheduled proportional to their votes.
        */
      share_type        votes;
      witness_schedule_type schedule = none; /// How the witness was scheduled the last time it was scheduled

      /**
        * These fields are used for the witness scheduling algorithm which uses
        * virtual time to ensure that all witnesses are given proportional time
        * for producing blocks.
        *
        * @ref votes is used to determine speed. The @ref virtual_scheduled_time is
        * the expected time at which this witness should complete a virtual lap which
        * is defined as the position equal to 1000 times MAXVOTES.
        *
        * virtual_scheduled_time = virtual_last_update + (1000*MAXVOTES - virtual_position) / votes
        *
        * Every time the number of votes changes the virtual_position and virtual_scheduled_time must
        * update.  There is a global current virtual_scheduled_time which gets updated every time
        * a witness is scheduled.  To update the virtual_position the following math is performed.
        *
        * virtual_position       = virtual_position + votes * (virtual_current_time - virtual_last_update)
        * virtual_last_update    = virtual_current_time
        * votes                  += delta_vote
        * virtual_scheduled_time = virtual_last_update + (1000*MAXVOTES - virtual_position) / votes
        *
        * @defgroup virtual_time Virtual Time Scheduling
        */
      ///@{
      fc::uint128       virtual_last_update;
      fc::uint128       virtual_position;
      fc::uint128       virtual_scheduled_time = fc::uint128_max_value();
      ///@}

      digest_type       last_work;

      /**
        * This field represents the Hive blockchain version the witness is running.
        */
      version           running_version;

      hardfork_version  hardfork_version_vote;
      time_point_sec    hardfork_time_vote = HIVE_GENESIS_TIME;

      int64_t           available_witness_account_subsidies = 0;
  };


  class witness_vote_object : public object< witness_vote_object_type, witness_vote_object >
  {
    CHAINBASE_OBJECT( witness_vote_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( witness_vote_object )

      account_name_type witness;
      account_name_type account;
  };

  class witness_schedule_object : public object< witness_schedule_object_type, witness_schedule_object >
  {
    CHAINBASE_OBJECT( witness_schedule_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( witness_schedule_object )

      fc::uint128                                        current_virtual_time;
      uint32_t                                           next_shuffle_block_num = 1;
      fc::array< account_name_type, HIVE_MAX_WITNESSES > current_shuffled_witnesses;
      uint8_t                                            num_scheduled_witnesses = 1;
      uint8_t                                            elected_weight = 1;
      uint8_t                                            timeshare_weight = 5;
      uint8_t                                            miner_weight = 1;
      uint32_t                                           witness_pay_normalization_factor = 25;
      chain_properties                                   median_props;
      version                                            majority_version;

      uint8_t max_voted_witnesses            = HIVE_MAX_VOTED_WITNESSES_HF0;
      uint8_t max_miner_witnesses            = HIVE_MAX_MINER_WITNESSES_HF0;
      uint8_t max_runner_witnesses           = HIVE_MAX_RUNNER_WITNESSES_HF0;
      uint8_t hardfork_required_witnesses    = HIVE_HARDFORK_REQUIRED_WITNESSES;

      // Derived fields that are stored for easy caching and reading of values.
      rd_dynamics_params account_subsidy_rd;
      rd_dynamics_params account_subsidy_witness_rd;
      int64_t            min_witness_account_subsidy_decay = 0;
      
      void copy_values_from(const witness_schedule_object& rhs)
      {
        current_virtual_time = rhs.current_virtual_time;

        next_shuffle_block_num = rhs.next_shuffle_block_num;
        current_shuffled_witnesses = rhs.current_shuffled_witnesses;
        num_scheduled_witnesses = rhs.num_scheduled_witnesses;
        elected_weight = rhs.elected_weight;
        timeshare_weight = rhs.timeshare_weight;
        miner_weight = rhs.miner_weight;
        witness_pay_normalization_factor = rhs.witness_pay_normalization_factor;
        median_props = rhs.median_props;
        majority_version = rhs.majority_version;

        max_voted_witnesses = rhs.max_voted_witnesses;
        max_miner_witnesses = rhs.max_miner_witnesses;
        max_runner_witnesses = rhs.max_runner_witnesses;
        hardfork_required_witnesses = rhs.hardfork_required_witnesses;

        // Derived fields that are stored for easy caching and reading of values.
        account_subsidy_rd = rhs.account_subsidy_rd;
        account_subsidy_witness_rd = rhs.account_subsidy_witness_rd;
        min_witness_account_subsidy_decay = rhs.min_witness_account_subsidy_decay;
      }
  };



  struct by_vote_name;
  struct by_pow;
  struct by_work;
  struct by_schedule_time;
  /**
    * @ingroup object_index
    */
  typedef multi_index_container<
    witness_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id > >,
      ordered_unique< tag< by_work >,
        composite_key< witness_object,
          member< witness_object, digest_type, &witness_object::last_work >,
          const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id >
        >
      >,
      ordered_unique< tag< by_name >,
        member< witness_object, account_name_type, &witness_object::owner > >,
      ordered_unique< tag< by_pow >,
        composite_key< witness_object,
          member< witness_object, uint64_t, &witness_object::pow_worker >,
          const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id >
        >
      >,
      ordered_unique< tag< by_vote_name >,
        composite_key< witness_object,
          member< witness_object, share_type, &witness_object::votes >,
          member< witness_object, account_name_type, &witness_object::owner >
        >,
        composite_key_compare< std::greater< share_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_schedule_time >,
        composite_key< witness_object,
          member< witness_object, fc::uint128, &witness_object::virtual_scheduled_time >,
          const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id >
        >
      >
    >,
    allocator< witness_object >
  > witness_index;

  struct by_account_witness;
  struct by_witness_account;
  typedef multi_index_container<
    witness_vote_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< witness_vote_object, witness_vote_object::id_type, &witness_vote_object::get_id > >,
      ordered_unique< tag< by_account_witness >,
        composite_key< witness_vote_object,
          member< witness_vote_object, account_name_type, &witness_vote_object::account >,
          member< witness_vote_object, account_name_type, &witness_vote_object::witness >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_witness_account >,
        composite_key< witness_vote_object,
          member< witness_vote_object, account_name_type, &witness_vote_object::witness >,
          member< witness_vote_object, account_name_type, &witness_vote_object::account >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >
    >, // indexed_by
    allocator< witness_vote_object >
  > witness_vote_index;

  typedef multi_index_container<
    witness_schedule_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< witness_schedule_object, witness_schedule_object::id_type, &witness_schedule_object::get_id > >
    >,
    allocator< witness_schedule_object >
  > witness_schedule_index;

} }

FC_REFLECT_ENUM( hive::chain::witness_object::witness_schedule_type, (elected)(timeshare)(miner)(none) )

FC_REFLECT( hive::chain::chain_properties,
          (account_creation_fee)
          (maximum_block_size)
          (hbd_interest_rate)
          (account_subsidy_budget)
          (account_subsidy_decay)
        )

FC_REFLECT( hive::chain::witness_object,
          (id)
          (owner)
          (created)
          (url)(votes)(schedule)(virtual_last_update)(virtual_position)(virtual_scheduled_time)(total_missed)
          (last_aslot)(last_confirmed_block_num)(pow_worker)(signing_key)
          (props)
          (hbd_exchange_rate)(last_hbd_exchange_update)
          (last_work)
          (running_version)
          (hardfork_version_vote)(hardfork_time_vote)
          (available_witness_account_subsidies)
        )
CHAINBASE_SET_INDEX_TYPE( hive::chain::witness_object, hive::chain::witness_index )

FC_REFLECT( hive::chain::witness_vote_object, (id)(witness)(account) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::witness_vote_object, hive::chain::witness_vote_index )

FC_REFLECT(hive::chain::witness_schedule_object,
           (id)(current_virtual_time)(next_shuffle_block_num)(current_shuffled_witnesses)(num_scheduled_witnesses)
           (elected_weight)(timeshare_weight)(miner_weight)(witness_pay_normalization_factor)
           (median_props)(majority_version)
           (max_voted_witnesses)
           (max_miner_witnesses)
           (max_runner_witnesses)
           (hardfork_required_witnesses)
           (account_subsidy_rd)
           (account_subsidy_witness_rd)
           (min_witness_account_subsidy_decay))
CHAINBASE_SET_INDEX_TYPE( hive::chain::witness_schedule_object, hive::chain::witness_schedule_index )
