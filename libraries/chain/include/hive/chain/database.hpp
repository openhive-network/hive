/*
  * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
  */
#pragma once
#include <hive/chain/block_flow_control.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/hardfork_property_object.hpp>
#include <hive/chain/node_property_object.hpp>
#include <hive/chain/notifications.hpp>

#include <hive/chain/util/advanced_benchmark_dumper.hpp>
#include <hive/chain/util/signal.hpp>

#include <hive/protocol/protocol.hpp>
#include <hive/protocol/hardfork.hpp>

#include <appbase/plugin.hpp>

#include <fc/signals.hpp>

#include <fc/log/logger.hpp>

#include <functional>
#include <map>

namespace hive {

namespace plugins {namespace chain
{
  class snapshot_dump_helper;
  class snapshot_load_helper;
} /// namespace chain

} /// namespace plugins

namespace chain {

  using hive::protocol::signed_transaction;
  using hive::protocol::operation;
  using hive::protocol::authority;
  using hive::protocol::asset;
  using hive::protocol::asset_symbol_type;
  using hive::protocol::price;
  using abstract_plugin = appbase::abstract_plugin;

  struct prepare_snapshot_supplement_notification;
  struct load_snapshot_supplement_notification;

  class database;

  struct hardfork_versions
  {
    fc::time_point_sec         times[ HIVE_NUM_HARDFORKS + 1 ];
    protocol::hardfork_version versions[ HIVE_NUM_HARDFORKS + 1 ];
  };

  class database_impl;
  class custom_operation_interpreter;
  class custom_operation_notification;

  namespace util {
    struct comment_reward_context;
  }

  namespace util {
    class advanced_benchmark_dumper;
  }

  struct reindex_notification;

  struct generate_optional_actions_notification {};

  typedef std::function<void(uint32_t, const chainbase::database::abstract_index_cntr_t&)> TBenchmarkMidReport;
  typedef std::pair<uint32_t, TBenchmarkMidReport> TBenchmark;

  struct open_args
  {
    fc::path data_dir;
    fc::path shared_mem_dir;
    uint64_t initial_supply = HIVE_INIT_SUPPLY;
    uint64_t hbd_initial_supply = HIVE_HBD_INIT_SUPPLY;
    uint64_t shared_file_size = 0;
    uint16_t shared_file_full_threshold = 0;
    uint16_t shared_file_scale_rate = 0;
    uint32_t chainbase_flags = 0;
    bool do_validate_invariants = false;
    bool benchmark_is_enabled = false;
    fc::variant database_cfg;
    bool replay_in_memory = false;
    std::vector< std::string > replay_memory_indices{};
    bool enable_block_log_compression = true;
    int block_log_compression_level = 15;

    // The following fields are only used on reindexing
    uint32_t stop_replay_at = 0;
    bool exit_after_replay = false;
    bool force_replay = false;
    bool validate_during_replay = false;
  };

  /**
    *   @class database
    *   @brief tracks the blockchain state in an extensible manner
    */
  class database : public chainbase::database
  {
    public:
      database();
      ~database();

      enum transaction_status
      {
        TX_STATUS_NONE       = 0x00, //outside any transaction processing
        TX_STATUS_UNVERIFIED = 0x01, //new transaction from API or P2P
        TX_STATUS_PENDING    = 0x02, //transaction that was verified by the node and is now pending (or popped)
        TX_STATUS_BLOCK      = 0x08, //during block processing
        TX_STATUS_INC_BLOCK  = TX_STATUS_BLOCK | TX_STATUS_UNVERIFIED, //while processing new block from API or P2P
        TX_STATUS_GEN_BLOCK  = TX_STATUS_BLOCK | TX_STATUS_PENDING //while producing new block
      };

      // block coming from API or P2P is validated for the first time, also newly produced or even reapplied but after switching fork
      bool is_validating_block() const { return _current_tx_status == TX_STATUS_INC_BLOCK; }
      // this node is a block producer and it creates new block out of pending transactions
      // (note that new block is not actually a block, that is, there are no pre/post block notifications)
      bool is_producing_block() const { return _current_tx_status == TX_STATUS_GEN_BLOCK; }
      // replying previously validated block (irreversible)
      bool is_replaying_block() const { return _current_tx_status == TX_STATUS_BLOCK; }
      // processing any block ( == is_validating_block() || is_producing_block() || is_replaying_block() )
      bool is_processing_block() const { return ( _current_tx_status & TX_STATUS_BLOCK ) != 0; }

      // transaction coming from API or P2P is validated for the first time, also as part of block to validate
      bool is_validating_tx() const { return ( _current_tx_status & TX_STATUS_UNVERIFIED ) != 0; }
      // transaction coming from API or P2P not as part of block is validated for the first time
      bool is_validating_one_tx() const { return _current_tx_status == TX_STATUS_UNVERIFIED; }
      // pending (or popped) transaction is now reapplied (also as part of new block)
      bool is_reapplying_tx() const { return ( _current_tx_status & TX_STATUS_PENDING ) != 0; }
      // pending (or popped) transaction is now reapplied not as part of block
      bool is_reapplying_one_tx() const { return _current_tx_status == TX_STATUS_PENDING; }

      // node has decisive power over what is acceptable (to propagate via P2P or include in new block)
      bool is_in_control() const { return is_validating_one_tx() || is_producing_block(); }

      transaction_status get_tx_status() const { return _current_tx_status; }

      void set_tx_status(transaction_status s)
      {
        if (_current_tx_status != TX_STATUS_NONE)
        {
          wlog("Nested tx processing: _current_tx_status==${cs}, incoming ${s}", ("cs", (int)_current_tx_status)("s", (int)s));
          // make sure to unconditionally call clear_tx_status() when processing ends or is broken
        }
        _current_tx_status = s;
      }
      void clear_tx_status() { _current_tx_status = TX_STATUS_NONE; }

      bool _log_hardforks = true;

      enum validation_steps
      {
        skip_nothing                = 0,
        skip_witness_signature      = 1 << 0,  ///< used while reindexing
        skip_transaction_signatures = 1 << 1,  ///< used by non-witness nodes
        skip_transaction_dupe_check = 1 << 2,  ///< used while reindexing
        skip_fork_db                = 1 << 3,  ///< used while reindexing
        skip_block_size_check       = 1 << 4,  ///< used when applying locally generated transactions
        skip_tapos_check            = 1 << 5,  ///< used while reindexing -- note this skips expiration check as well
        skip_authority_check        = 1 << 6,  ///< used while reindexing -- disables any checking of authority on transactions
        skip_merkle_check           = 1 << 7,  ///< used while reindexing
        skip_undo_history_check     = 1 << 8,  ///< used while reindexing
        skip_witness_schedule_check = 1 << 9,  ///< used while reindexing
        skip_validate               = 1 << 10, ///< used prior to checkpoint, skips validate() call on transaction
        skip_validate_invariants    = 1 << 11, ///< used to skip database invariant check on block application
        skip_undo_block             = 1 << 12, ///< used to skip undo db on reindex
        skip_block_log              = 1 << 13  ///< used to skip block logging on reindex
      };

      /**
        * @brief Open a database, creating a new one if necessary
        *
        * Opens a database in the specified directory. If no initialized database is found the database
        * will be initialized with the default state.
        *
        * @param data_dir Path to open or create database in
        */
      void open( const open_args& args );

    private:

      uint32_t reindex_internal( const open_args& args, const std::shared_ptr<full_block_type>& full_block );
      void remove_expired_governance_votes();

      /// Allows to load all data being independent to the persistent storage held in shared memory file.
      void initialize_state_independent_data(const open_args& args);

      bool is_included_block_unlocked(const block_id_type& block_id);
    public:
      std::vector<block_id_type> get_blockchain_synopsis(const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point);
      std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received);
      std::vector<block_id_type> get_block_ids(const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit);

      /// Allows to load all required initial data from persistent storage held in shared memory file. Must be used directly after opening a database, but also after loading a snapshot.
      void load_state_initial_data(const open_args& args);

      /**
        * @brief Check if replaying was finished and all blocks from `block_log` were processed.
        *
        * This method is called from a chain plugin, if returns `true` then a synchronization is allowed.
        * If returns `false`, then opening a node should be forbidden.
        *
        * There are output-type arguments: `head_block_num_origin`, `head_block_num_state` for information purposes only.
        *
        * @return information if replaying was finished
        */
      bool is_reindex_complete( uint64_t* head_block_num_origin, uint64_t* head_block_num_state ) const;

      /**
        * @brief Rebuild object graph from block history and open detabase
        *
        * This method may be called after or instead of @ref database::open, and will rebuild the object graph by
        * replaying blockchain history. When this method exits successfully, the database will be open.
        *
        * @return the last replayed block number.
        */
      uint32_t reindex( const open_args& args );

      /**
        * @brief wipe Delete database from disk, and potentially the raw chain as well.
        * @param include_blocks If true, delete the raw chain as well as the database.
        *
        * Will close the database before wiping. Database will be closed when this function returns.
        */
      void wipe(const fc::path& data_dir, const fc::path& shared_mem_dir, bool include_blocks);
      void close(bool rewind = true);

      //////////////////// db_block.cpp ////////////////////

      /**
        *  @return true if the block is in our fork DB or saved to disk as
        *  part of the official chain, otherwise return false
        */
      bool                       is_known_block( const block_id_type& id )const;
    private:
      bool                       is_known_block_unlocked(const block_id_type& id)const;
    public:
      bool                       is_known_transaction( const transaction_id_type& id )const;
      fc::sha256                 get_pow_target()const;
      uint32_t                   get_pow_summary_target()const;
      block_id_type              find_block_id_for_num( uint32_t block_num )const;
    public:
      block_id_type              get_block_id_for_num( uint32_t block_num )const;
      std::shared_ptr<full_block_type> fetch_block_by_id(const block_id_type& id)const;
      std::shared_ptr<full_block_type> fetch_block_by_number( uint32_t num, fc::microseconds wait_for_microseconds = fc::microseconds() )const;
      std::vector<std::shared_ptr<full_block_type>>  fetch_block_range( const uint32_t starting_block_num, const uint32_t count, 
                                                                        fc::microseconds wait_for_microseconds = fc::microseconds() );
      std::vector<block_id_type> get_block_ids_on_fork(block_id_type head_of_fork) const;

      /// Warning: to correctly process old blocks initially old chain-id should be set.
      chain_id_type hive_chain_id = OLD_CHAIN_ID;
      /// Returns current chain-id being in use depending on applied HF
      chain_id_type get_chain_id() const;
      /// Returns pre-HF24 chain id (if mainnet is used).
      chain_id_type get_old_chain_id() const;
      /// Returns post-HF24 chain id (if mainnet is used).
      chain_id_type get_new_chain_id() const;

      void set_chain_id( const chain_id_type& chain_id );

      const witness_object&  get_witness(  const account_name_type& name )const;
      const witness_object*  find_witness( const account_name_type& name )const;

      /// Gives name of account with NO authority which holds resources for payouts according to proposals (at a time of given hardfork)
      std::string            get_treasury_name( uint32_t hardfork )const;
      std::string            get_treasury_name()const { return get_treasury_name( get_hardfork() ); }
      const account_object&  get_treasury()const { return get_account( get_treasury_name() ); }
      /// Returns true for any account name that was ever a treasury account
      bool                   is_treasury( const account_name_type& name )const;

      const account_object&  get_account(  const account_id_type      id )const;
      const account_object*  find_account( const account_id_type&     id )const;

      const account_object&  get_account(  const account_name_type& name )const;
      const account_object*  find_account( const account_name_type& name )const;

      const comment_object&  get_comment( comment_id_type comment_id )const;

      const comment_object&  get_comment(  const account_id_type& author, const shared_string& permlink )const;
      const comment_object*  find_comment( const account_id_type& author, const shared_string& permlink )const;

      const comment_object&  get_comment(  const account_name_type& author, const shared_string& permlink )const;
      const comment_object*  find_comment( const account_name_type& author, const shared_string& permlink )const;

#ifndef ENABLE_STD_ALLOCATOR
      const comment_object&  get_comment(  const account_id_type& author, const string& permlink )const;
      const comment_object*  find_comment( const account_id_type& author, const string& permlink )const;

      const comment_object&  get_comment(  const account_name_type& author, const string& permlink )const;
      const comment_object*  find_comment( const account_name_type& author, const string& permlink )const;
#endif

      const escrow_object&   get_escrow(  const account_name_type& name, uint32_t escrow_id )const;
      const escrow_object*   find_escrow( const account_name_type& name, uint32_t escrow_id )const;

      const limit_order_object& get_limit_order(  const account_name_type& owner, uint32_t id )const;
      const limit_order_object* find_limit_order( const account_name_type& owner, uint32_t id )const;

      const savings_withdraw_object& get_savings_withdraw(  const account_name_type& owner, uint32_t request_id )const;
      const savings_withdraw_object* find_savings_withdraw( const account_name_type& owner, uint32_t request_id )const;

      const dynamic_global_property_object&  get_dynamic_global_properties()const;
      const node_property_object&            get_node_properties()const;
      const feed_history_object&             get_feed_history()const;
      const witness_schedule_object&         get_witness_schedule_object()const;
      const witness_schedule_object&         get_future_witness_schedule_object() const;

      const hardfork_property_object&        get_hardfork_property_object()const;

    private:

      const comment_object&                  get_comment_for_payout_time( const comment_object& comment )const;

    public:

      const time_point_sec                   calculate_discussion_payout_time( const comment_object& comment )const;
      const time_point_sec                   calculate_discussion_payout_time( const comment_object& comment, const comment_cashout_object& comment_cashout )const;
      const reward_fund_object&              get_reward_fund()const;

      const comment_cashout_object* find_comment_cashout( const comment_object& comment ) const;
      const comment_cashout_object* find_comment_cashout( comment_id_type comment_id ) const;
      const comment_cashout_ex_object* find_comment_cashout_ex( const comment_object& comment ) const;
      const comment_cashout_ex_object* find_comment_cashout_ex( comment_id_type comment_id ) const;
      const comment_object& get_comment( const comment_cashout_object& comment_cashout ) const;
      void remove_old_cashouts();

      asset get_effective_vesting_shares( const account_object& account, asset_symbol_type vested_symbol )const;

      void max_bandwidth_per_share()const;

      /**
        *  Calculate the percent of block production slots that were missed in the
        *  past 128 blocks, not including the current block.
        */
      uint32_t witness_participation_rate()const;

      void                                   add_checkpoints( const flat_map<uint32_t,block_id_type>& checkpts );
      const flat_map<uint32_t,block_id_type> get_checkpoints()const { return _checkpoints; }
      bool                                   before_last_checkpoint()const;

      bool push_block( const block_flow_control& block_ctrl, uint32_t skip = skip_nothing );
      void push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip = skip_nothing );
      void _maybe_warn_multiple_production( uint32_t height )const;
      bool _push_block( const block_flow_control& block_ctrl );
      void _push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction );

      void pop_block();
      void clear_pending();

      void push_virtual_operation( const operation& op );
      void pre_push_virtual_operation( const operation& op );
      void post_push_virtual_operation( const operation& op, const fc::optional<uint64_t>& op_in_trx = fc::optional<uint64_t>() );

      /*
        * Pushing an action without specifying an execution time will execute at head block.
        * The execution time must be greater than or equal to head block.
        */
      void push_required_action( const required_automated_action& a, time_point_sec execution_time );
      void push_required_action( const required_automated_action& a );

      void push_optional_action( const optional_automated_action& a, time_point_sec execution_time );
      void push_optional_action( const optional_automated_action& a );

      void notify_pre_apply_required_action( const required_action_notification& note );
      void notify_post_apply_required_action( const required_action_notification& note );

      void notify_pre_apply_optional_action( const optional_action_notification& note );
      void notify_post_apply_optional_action( const optional_action_notification& note );
      /**
        *  This method is used to track applied operations during the evaluation of a block, these
        *  operations should include any operation actually included in a transaction as well
        *  as any implied/virtual operations that resulted, such as filling an order.
        *  The applied operations are cleared after post_apply_operation.
        */
      void notify_pre_apply_operation( const operation_notification& note );
      void notify_post_apply_operation( const operation_notification& note );
      void notify_pre_apply_block( const block_notification& note );
      void notify_post_apply_block( const block_notification& note );
      void notify_fail_apply_block( const block_notification& note );
      void notify_irreversible_block( uint32_t block_num );
      void notify_switch_fork( uint32_t block_num );
      void notify_pre_apply_transaction( const transaction_notification& note );
      void notify_post_apply_transaction( const transaction_notification& note );
      void notify_pre_apply_custom_operation( const custom_operation_notification& note );
      void notify_post_apply_custom_operation( const custom_operation_notification& note );


      using apply_required_action_handler_t = std::function< void(const required_action_notification&) >;
      using apply_optional_action_handler_t = std::function< void(const optional_action_notification&) >;
      using apply_operation_handler_t = std::function< void(const operation_notification&) >;
      using apply_transaction_handler_t = std::function< void(const transaction_notification&) >;
      using apply_block_handler_t = std::function< void(const block_notification&) >;
      using apply_custom_operation_handler_t = std::function< void(const custom_operation_notification&) >;
      using irreversible_block_handler_t = std::function< void(uint32_t) >;
      using switch_fork_handler_t = std::function< void(uint32_t) >;
      using reindex_handler_t = std::function< void(const reindex_notification&) >;
      using generate_optional_actions_handler_t = std::function< void(const generate_optional_actions_notification&) >;
      using prepare_snapshot_handler_t = std::function < void(const database&, const database::abstract_index_cntr_t&)>;
      using prepare_snapshot_data_supplement_handler_t = std::function < void(const prepare_snapshot_supplement_notification&) >;
      using load_snapshot_data_supplement_handler_t = std::function < void(const load_snapshot_supplement_notification&) >;
      using comment_reward_notification_handler_t = std::function < void(const comment_reward_notification&) >;
      using end_of_syncing_notification_handler_t = std::function < void(void) >;

      void notify_prepare_snapshot_data_supplement(const prepare_snapshot_supplement_notification& n);
      void notify_load_snapshot_data_supplement(const load_snapshot_supplement_notification& n);
      void notify_comment_reward(const comment_reward_notification& note);
      void notify_end_of_syncing();

    private:
      template < bool IS_PRE_OPERATION, typename TSignal,
                 typename TNotification = std::function<typename TSignal::signature_type> >
      boost::signals2::connection connect_impl( TSignal& signal, const TNotification& func,
        const abstract_plugin& plugin, int32_t group, const std::string& item_name = "" );

      template< bool IS_PRE_OPERATION >
      boost::signals2::connection any_apply_operation_handler_impl( const apply_operation_handler_t& func,
        const abstract_plugin& plugin, int32_t group );

    public:

      boost::signals2::connection add_pre_apply_required_action_handler ( const apply_required_action_handler_t&     func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_apply_required_action_handler( const apply_required_action_handler_t&     func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_pre_apply_optional_action_handler ( const apply_optional_action_handler_t&     func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_apply_optional_action_handler( const apply_optional_action_handler_t&     func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_pre_apply_operation_handler       ( const apply_operation_handler_t&           func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_apply_operation_handler      ( const apply_operation_handler_t&           func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_pre_apply_transaction_handler     ( const apply_transaction_handler_t&         func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_apply_transaction_handler    ( const apply_transaction_handler_t&         func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_pre_apply_block_handler           ( const apply_block_handler_t&               func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_apply_block_handler          ( const apply_block_handler_t&               func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_fail_apply_block_handler          ( const apply_block_handler_t&               func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_irreversible_block_handler        ( const irreversible_block_handler_t&        func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_switch_fork_handler               ( const switch_fork_handler_t&        func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_pre_reindex_handler               ( const reindex_handler_t&                   func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_reindex_handler              ( const reindex_handler_t&                   func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_generate_optional_actions_handler ( const generate_optional_actions_handler_t& func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_pre_apply_custom_operation_handler ( const apply_custom_operation_handler_t&    func, const abstract_plugin& plugin, int32_t group = -1 );
      boost::signals2::connection add_post_apply_custom_operation_handler( const apply_custom_operation_handler_t&    func, const abstract_plugin& plugin, int32_t group = -1 );


      boost::signals2::connection add_prepare_snapshot_handler          (const prepare_snapshot_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);
      /// <summary>
      ///  All plugins storing data in different way than chainbase::generic_index (wrapping
      ///  a multi_index) should register to this handler to add its own data to the prepared snapshot.
      /// </summary>
      /// <param name="func">function to be called on notification</param>
      /// <param name="plugin">the plugin be registering its handler</param>
      /// <param name="group"></param>
      /// <returns></returns>
      boost::signals2::connection add_snapshot_supplement_handler       (const prepare_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);
      /// <summary>
      ///  All plugins storing data in different way than chainbase::generic_index (wrapping
      ///  a multi_index) should register to this handler to load its own data from the loaded snapshot.
      /// </summary>
      /// <param name="func"></param>
      /// <param name="plugin"></param>
      /// <param name="group"></param>
      /// <returns></returns>
      boost::signals2::connection add_snapshot_supplement_handler       (const load_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);

      boost::signals2::connection add_comment_reward_handler            (const comment_reward_notification_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);

      boost::signals2::connection add_end_of_syncing_handler            (const end_of_syncing_notification_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);

      //////////////////// db_witness_schedule.cpp ////////////////////

      /**
        * @brief Get the witness scheduled for block production in a slot.
        *
        * slot_num always corresponds to a time in the future.
        *
        * If slot_num == 1, returns the next scheduled witness.
        * If slot_num == 2, returns the next scheduled witness after
        * 1 block gap.
        *
        * Use the get_slot_time() and get_slot_at_time() functions
        * to convert between slot_num and timestamp.
        */
      account_name_type get_scheduled_witness(uint32_t slot_num)const;

      /**
        * Get the time at which the given slot occurs.
        *
        * If slot_num == 0, return time_point_sec().
        *
        * If slot_num == N for N > 0, return the Nth next
        * block-interval-aligned time greater than head_block_time().
        */
      fc::time_point_sec get_slot_time(uint32_t slot_num)const;

      /**
        * Get the last slot which occurs AT or BEFORE the given time.
        *
        * The return value is the greatest value N such that
        * get_slot_time( N ) <= when.
        *
        * If no such N exists, return 0.
        */
      uint32_t get_slot_at_time(fc::time_point_sec when)const;

      /** @return the HBD created and deposited to_account, may return HIVE if there is no median feed */
      std::pair< asset, asset > create_hbd( const account_object& to_account, asset hive, bool to_reward_balance=false );

      using Before = std::function< void( const asset& ) >;
      asset adjust_account_vesting_balance(const account_object& to_account, const asset& liquid, bool to_reward_balance, Before&& before_vesting_callback );

      asset create_vesting( const account_object& to_account, const asset& liquid, bool to_reward_balance=false );

      void adjust_liquidity_reward( const account_object& owner, const asset& volume, bool is_hbd );

      void adjust_balance( const account_object& a, const asset& delta );
      void adjust_balance( const account_name_type& name, const asset& delta )
      {
        adjust_balance( get_account( name ), delta );
      }

      void adjust_savings_balance( const account_object& a, const asset& delta );

      void adjust_reward_balance( const account_object& a, const asset& value_delta, const asset& share_delta = asset(0,VESTS_SYMBOL) );
      void adjust_reward_balance( const account_name_type& name, const asset& value_delta, const asset& share_delta = asset(0,VESTS_SYMBOL) )
      {
        adjust_reward_balance( get_account( name ), value_delta, share_delta );
      }

      void adjust_supply( const asset& delta, bool adjust_vesting = false );
      void adjust_rshares2( fc::uint128_t old_rshares2, fc::uint128_t new_rshares2 );
      void update_owner_authority( const account_object& account, const authority& owner_authority );

      asset get_balance( const account_object& a, asset_symbol_type symbol )const;
      asset get_savings_balance( const account_object& a, asset_symbol_type symbol )const;
      asset get_balance( const account_name_type& aname, asset_symbol_type symbol )const
      {
        return get_balance( get_account( aname ), symbol );
      }

      /** this updates the votes for witnesses as a result of account voting proxy changing */
      void adjust_proxied_witness_votes( const account_object& a,
                              const std::array< share_type, HIVE_MAX_PROXY_RECURSION_DEPTH+1 >& delta,
                              int depth = 0 );

      /** this updates the votes for all witnesses as a result of account VESTS changing */
      void adjust_proxied_witness_votes( const account_object& a, share_type delta, int depth = 0 );
      /** like above with delta that negates all vote power for given user - both for individual witnesses and/or proxy */
      void nullify_proxied_witness_votes( const account_object& a );

      /** this is called by `adjust_proxied_witness_votes` when account proxy to self */
      void adjust_witness_votes( const account_object& a, const share_type& delta );

      /** this updates the vote of a single witness as a result of a vote being added or removed*/
      void adjust_witness_vote( const witness_object& obj, share_type delta );

      /** clears all vote records for a particular account but does not update the
        * witness vote totals.  Vote totals should be updated first via a call to
        * nullify_proxied_witness_votes( a )
        */
      void clear_witness_votes( const account_object& a );
      void process_vesting_withdrawals();
      share_type pay_curators( const comment_object& comment, const comment_cashout_object& comment_cashout, share_type& max_rewards );
      share_type cashout_comment_helper( util::comment_reward_context& ctx, const comment_object& comment,
        const comment_cashout_object& comment_cashout, const comment_cashout_ex_object* comment_cashout_ex,
        bool forward_curation_remainder = true );
      void process_comment_cashout();
      void process_funds();
      void process_conversions();
      void process_savings_withdraws();
      void process_subsidized_accounts();
      void account_recovery_processing();
      void expire_escrow_ratification();
      void process_decline_voting_rights();
      void update_median_feed();

      asset get_liquidity_reward()const;
      asset get_content_reward()const;
      asset get_producer_reward();
      asset get_curation_reward()const;
      asset get_pow_reward()const;

      uint16_t get_curation_rewards_percent() const;

      share_type pay_reward_funds( const share_type& reward );

      void  pay_liquidity_reward();

      /**
        * Helper method to return the current HBD value of a given amount of
        * HIVE.  Return 0 HBD if there isn't a current_median_history
        */
      asset to_hbd( const asset& hive )const;
      asset to_hive( const asset& hbd )const;

      time_point_sec   head_block_time()const;
      uint32_t         head_block_num()const;
      block_id_type    head_block_id()const;

      time_point_sec   head_block_time_from_fork_db(fc::microseconds wait_for_microseconds = fc::microseconds())const;
      uint32_t         head_block_num_from_fork_db(fc::microseconds wait_for_microseconds = fc::microseconds())const;
      block_id_type    head_block_id_from_fork_db(fc::microseconds wait_for_microseconds = fc::microseconds())const;

      node_property_object& node_properties();

      uint32_t get_last_irreversible_block_num()const;
      void set_last_irreversible_block_num(uint32_t block_num);
      struct irreversible_object_type
      {
        uint32_t last_irreversible_block_num = 0;
      } *irreversible_object = nullptr;
      //////////////////// db_init.cpp ////////////////////

      void initialize_evaluators();
      void register_custom_operation_interpreter( std::shared_ptr< custom_operation_interpreter > interpreter );
      std::shared_ptr< custom_operation_interpreter > get_custom_json_evaluator( const custom_id_type& id );

      /// Reset the object graph in-memory
      void initialize_indexes();

      // Reset irreversible state (unaffected by undo)
      void initialize_irreversible_storage();

      void resetState(const open_args& args);

      void init_schema();
      void init_genesis(uint64_t initial_supply = HIVE_INIT_SUPPLY, uint64_t hbd_initial_supply = HIVE_HBD_INIT_SUPPLY );

      /** when popping a block, the transactions that were removed get cached here so they
        * can be reapplied at the proper time */
      std::deque<std::shared_ptr<full_transaction_type>>       _popped_tx;
      vector<std::shared_ptr<full_transaction_type>>           _pending_tx;

      bool apply_order( const limit_order_object& new_order_object );
      bool fill_order( const limit_order_object& order, const asset& pays, const asset& receives );
      void cancel_order( const limit_order_object& obj, bool suppress_vop = false );
      int  match( const limit_order_object& bid, const limit_order_object& ask, const price& trade_price );

      void perform_vesting_share_split( uint32_t magnitude );
      void retally_witness_votes();
      void retally_witness_vote_counts( bool force = false );
      void retally_liquidity_weight();
      uint16_t calculate_HBD_percent();
      void update_virtual_supply();

      bool has_hardfork( uint32_t hardfork )const;

      uint32_t get_hardfork()const;

      /* For testing and debugging only. Given a hardfork
        with id N, applies all hardforks with id <= N */
      void set_hardfork( uint32_t hardfork, bool process_now = true );

      void validate_invariants()const;
      /**
        * @}
        */

      const std::string& get_json_schema() const;

      void set_flush_interval( uint32_t flush_blocks );
      void check_free_memory( bool force_print, uint32_t current_block_num );

      void apply_transaction( const std::shared_ptr<full_transaction_type>& trx, uint32_t skip = skip_nothing );
      void apply_required_action( const required_automated_action& a );
      void apply_optional_action( const optional_automated_action& a );

      optional< chainbase::database::session >& pending_transaction_session();

#ifdef IS_TEST_NET
      bool liquidity_rewards_enabled = true;
      bool skip_price_feed_limit_check = true;
      bool skip_transaction_delta_check = true;
      bool disable_low_mem_warning = true;
#endif

#ifdef HIVE_ENABLE_SMT
      ///Smart Media Tokens related methods
      ///@{
      void validate_smt_invariants()const;
      ///@}
#endif

      //Restores balances for some accounts, which were cleared by mistake during HF23
      void restore_accounts( const std::set< std::string >& restored_accounts );

      //Clears all pending operations on account that involve balance, moves tokens to treasury account
      void gather_balance( const std::string& name, const asset& balance, const asset& hbd_balance );
      void clear_accounts( const std::set< std::string >& cleared_accounts );
      void clear_account( const account_object& account );

      // return the witness schedule object whose current_shuffled_witnesses we use for computing irreversibility.  Roughly, before HF26, it's the 
      // current witnesses_schedule_object; after, it's future_witness_schedule_object
      const witness_schedule_object& get_witness_schedule_object_for_irreversibility() const;
    protected:
      //Mark pop_undo() as protected -- we do not want outside calling pop_undo(); it should call pop_block() instead
      //void pop_undo() { object_database::pop_undo(); }
      void notify_changed_objects();

    private:
      optional< chainbase::database::session > _pending_tx_session;

      void apply_block(const std::shared_ptr<full_block_type>& full_block, uint32_t skip = skip_nothing );
      void switch_forks(item_ptr new_head);
      void _apply_block(const std::shared_ptr<full_block_type>& full_block);
      void validate_transaction(const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip);
      void _apply_transaction( const std::shared_ptr<full_transaction_type>& trx );
      void apply_operation( const operation& op );

      void process_required_actions( const required_automated_actions& actions );
      void process_optional_actions( const optional_automated_actions& actions );

      ///Steps involved in applying a new block
      ///@{

      const witness_object& validate_block_header( uint32_t skip, const std::shared_ptr<full_block_type>& full_block )const;
      void create_block_summary(const std::shared_ptr<full_block_type>& full_block);

      //calculates sum of all balances stored on given account, returns true if any is nonzero
      bool collect_account_total_balance( const account_object& account, asset* total_hive, asset* total_hbd,
        asset* total_vests, asset* vesting_shares_hive_value );
      //removes (burns) balances held on null account
      void clear_null_account_balance();
      //moves balances from old treasury account to current one
      void consolidate_treasury_balance();

      //locks given account by clearing its authorizations and removing pending recovery [account change] request (used for treasury in HF code)
      void lock_account( const account_object& account );

      void process_proposals( const block_notification& note );

      void process_delayed_voting(const block_notification& note );

      void process_recurrent_transfers();

      void update_global_dynamic_data( const signed_block& b );
      void update_signing_witness(const witness_object& signing_witness, const signed_block& new_block);
      void process_fast_confirm_transaction(const std::shared_ptr<full_transaction_type>& full_transaction);
      uint32_t update_last_irreversible_block();
      void migrate_irreversible_state(uint32_t old_last_irreversible);
      void clear_expired_transactions();
      void clear_expired_orders();
      void clear_expired_delegations();
      void process_header_extensions( const signed_block& next_block, required_automated_actions& req_actions, optional_automated_actions& opt_actions );
      void process_genesis_accounts();

      void generate_required_actions();
      void generate_optional_actions();

      void init_hardforks();
      void process_hardforks();
      void apply_hardfork( uint32_t hardfork );

      ///@}
#ifdef HIVE_ENABLE_SMT
      template< typename smt_balance_object_type, typename modifier_type >
      void adjust_smt_balance( const account_object& owner, const asset& delta, modifier_type&& modifier );
#endif
      void modify_balance( const account_object& a, const asset& delta, bool check_balance );
      void modify_reward_balance( const account_object& a, const asset& value_delta, const asset& share_delta, bool check_balance );

      operation_notification create_operation_notification( const operation& op )const
      {
        operation_notification note(op);
        note.trx_id       = _current_trx_id;
        note.block        = _current_block_num;
        note.trx_in_block = _current_trx_in_block;
        note.op_in_trx    = _current_op_in_trx;
        note.virtual_op   = hive::protocol::is_virtual_operation(op);
        return note;
      }

    public:

      const transaction_id_type& get_current_trx() const
      {
        return _current_trx_id;
      }
      uint16_t get_current_op_in_trx() const
      {
        return _current_op_in_trx;
      }

      int16_t get_remove_threshold() const
      {
        return get_dynamic_global_properties().current_remove_threshold;
      }

      void set_remove_threshold( int16_t val )
      {
        modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.current_remove_threshold = val;
        } );
      }

      bool get_snapshot_loaded() const
      {
        return snapshot_loaded;
      }

      void set_snapshot_loaded()
      {
        snapshot_loaded = true;
      }

      util::advanced_benchmark_dumper& get_benchmark_dumper()
      {
        return _benchmark_dumper;
      }

      const hardfork_versions& get_hardfork_versions()
      {
        return _hardfork_versions;
      }

    private:

      std::unique_ptr< database_impl > _my;

      fork_database                 _fork_db;
      hardfork_versions             _hardfork_versions;

      block_log                     _block_log;

      // this function needs access to _plugin_index_signal
      template< typename MultiIndexType >
      friend void add_plugin_index( database& db );

      transaction_status            _current_tx_status = TX_STATUS_NONE;
      transaction_id_type           _current_trx_id;
      uint32_t                      _current_block_num    = 0;
      int32_t                       _current_trx_in_block = 0;
      uint32_t                      _current_op_in_trx    = 0;

      const struct operation_notification* _current_applied_operation_info = nullptr;

      optional< block_id_type >     _currently_processing_block_id;

      flat_map<uint32_t,block_id_type>  _checkpoints;

      node_property_object              _node_property_object;

      uint32_t                      _flush_blocks = 0;
      uint32_t                      _next_flush_block = 0;

      uint32_t                      _last_free_gb_printed = 0;

      uint16_t                      _shared_file_full_threshold = 0;
      uint16_t                      _shared_file_scale_rate = 0;

      bool                          snapshot_loaded = false;

      flat_map< custom_id_type, std::shared_ptr< custom_operation_interpreter > >   _custom_operation_interpreters;
      std::string                   _json_schema;

      util::advanced_benchmark_dumper  _benchmark_dumper;

      fc::signal<void(const required_action_notification&)> _pre_apply_required_action_signal;
      fc::signal<void(const required_action_notification&)> _post_apply_required_action_signal;

      fc::signal<void(const optional_action_notification&)> _pre_apply_optional_action_signal;
      fc::signal<void(const optional_action_notification&)> _post_apply_optional_action_signal;

      fc::signal<void(const operation_notification&)>       _pre_apply_operation_signal;
      /**
        *  This signal is emitted for plugins to process every operation after it has been fully applied.
        */
      fc::signal<void(const operation_notification&)>       _post_apply_operation_signal;

      fc::signal<void(const custom_operation_notification&)> _pre_apply_custom_operation_signal;
      fc::signal<void(const custom_operation_notification&)> _post_apply_custom_operation_signal;

    /**
        *  This signal is emitted when we start processing a block.
        *
        *  You may not yield from this callback because the blockchain is holding
        *  the write lock and may be in an "inconstant state" until after it is
        *  released.
        */
      fc::signal<void(const block_notification&)>           _pre_apply_block_signal;

      fc::signal<void(uint32_t)>                            _on_irreversible_block;

      fc::signal<void(uint32_t)>                            _switch_fork_signal;

      /**
        *  This signal is emitted after all operations and virtual operation for a
        *  block have been applied but before the get_applied_operations() are cleared.
        *
        *  You may not yield from this callback because the blockchain is holding
        *  the write lock and may be in an "inconstant state" until after it is
        *  released.
        */
      fc::signal<void(const block_notification&)>           _post_apply_block_signal;

      /**
        *  This signal is emitted when any problems occured during block processing
        */
      fc::signal<void(const block_notification&)>           _fail_apply_block_signal;

      /**
        * This signal is emitted any time a new transaction is about to be applied
        * to the chain state.
        */
      fc::signal<void(const transaction_notification&)>     _pre_apply_transaction_signal;

      /**
        * This signal is emitted any time a new transaction has been applied to the
        * chain state.
        */
      fc::signal<void(const transaction_notification&)>     _post_apply_transaction_signal;

      /**
        * Emitted when reindexing starts
        */
      fc::signal<void(const reindex_notification&)>         _pre_reindex_signal;

      /**
        * Emitted when reindexing finishes
        */
      fc::signal<void(const reindex_notification&)>         _post_reindex_signal;

      fc::signal<void(const generate_optional_actions_notification& )> _generate_optional_actions_signal;

      fc::signal<void(const database&, const database::abstract_index_cntr_t&)> _prepare_snapshot_signal;

      /// <summary>
      ///  Emitted by snapshot plugin implementation to allow all registered plugins to include theirs custom-stored data in the snapshot
      /// </summary>
      fc::signal<void(const prepare_snapshot_supplement_notification&)> _prepare_snapshot_supplement_signal;
      /// <summary>
      /// Emitted by snapshot plugin implementation to allow all registered plugins to load theirs custom-stored data from the snapshot
      /// </summary>
      fc::signal<void(const load_snapshot_supplement_notification&)> _load_snapshot_supplement_signal;

      /**
        *  Emitted After a block has been applied and committed.  The callback
        *  should not yield and should execute quickly.
        */
      //fc::signal<void(const vector< object_id_type >&)> changed_objects;

      /** this signal is emitted any time an object is removed and contains a
        * pointer to the last value of every object that was removed.
        */
      //fc::signal<void(const vector<const object*>&)>  removed_objects;

      /**
        * Internal signal to execute deferred registration of plugin indexes.
        */
      fc::signal<void()>                                    _plugin_index_signal;

      /// <summary>
      ///  Emitted when rewards for author and curators are paid out.
      /// </summary>
      fc::signal<void(const comment_reward_notification&)>          _comment_reward_signal;

      fc::signal<void()> _end_of_syncing_signal;
  };

  struct reindex_notification
  {
    reindex_notification( const open_args& a ) : args( a ) {}

    bool force_replay = false;
    bool validate_during_replay = false;
    bool reindex_success = false;
    uint32_t last_block_number = 0;
    const open_args& args;
    uint32_t max_block_number = 0;
  };

  struct prepare_snapshot_supplement_notification
  {
    prepare_snapshot_supplement_notification(const fc::path& _external_data_storage_base_path, hive::plugins::chain::snapshot_dump_helper& _dump_helper) :
      external_data_storage_base_path(_external_data_storage_base_path), dump_helper(_dump_helper) {}
    fc::path              external_data_storage_base_path;
    hive::plugins::chain::snapshot_dump_helper& dump_helper;
  };

  struct load_snapshot_supplement_notification
  {
    explicit load_snapshot_supplement_notification(hive::plugins::chain::snapshot_load_helper& _load_helper) :
      load_helper(_load_helper) {}

    hive::plugins::chain::snapshot_load_helper& load_helper;
  };
} }
