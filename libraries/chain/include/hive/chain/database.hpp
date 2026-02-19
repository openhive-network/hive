/*
  * Copyright (c) 2020-2026 Hive blockchain community contributors.
  * Hive is a decentralized, open-source blockchain project maintained by its community.
  *
  * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
  */
#pragma once
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/node_property_object.hpp>

#include <hive/chain/util/advanced_benchmark_dumper.hpp>
#include <hive/chain/util/type_registrar.hpp>
#include <hive/chain/external_storage/comments_handler_ptr.hpp>
#include <hive/chain/external_storage/comment.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/block_header.hpp>

namespace hive { namespace protocol {
  struct signed_block;
  struct signed_transaction;
  struct authority;
  struct price;
} }

#include <hive/utilities/signal_connection_ptr.hpp>

#include <fc/reflect/reflect.hpp>

#include <fc/log/logger.hpp>

#include <functional>
#include <map>

namespace appbase {
  class application;
  class abstract_plugin;
}

namespace hive {

namespace plugins {namespace chain
{
  class snapshot_dump_helper;
  class snapshot_load_helper;
} /// namespace chain

} /// namespace plugins

namespace chain {

  using hive::protocol::signed_transaction;
  using hive::protocol::signed_block;
  using hive::protocol::authority;
  using hive::protocol::asset;
  using hive::protocol::asset_symbol_type;
  using hive::protocol::custom_id_type;
  using hive::protocol::price;
  using hive::protocol::hardfork_version;
  using hive::protocol::block_header_extensions;
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
  class hardfork_property_object;
  class block_write_i;
  class block_flow_control;
  namespace util { struct rd_dynamics_params; }

  struct block_notification;
  struct transaction_notification;
  struct operation_notification;
  struct comment_reward_notification;

  class blockchain_worker_thread_pool;
  class resource_credits;
  struct full_transaction_type;
  struct full_block_type;
  struct irreversible_block_data_type;

  namespace util {
    struct comment_reward_context;
  }

  namespace util {
    class advanced_benchmark_dumper;
  }

  struct reindex_notification;

  typedef std::function<void(uint32_t, const chainbase::database::abstract_index_cntr_t&)> TBenchmarkMidReport;
  typedef std::pair<uint32_t, TBenchmarkMidReport> TBenchmark;

  struct open_args
  {
    fc::path data_dir;
    fc::path shared_mem_dir;
    fc::path comments_storage_path;
    uint64_t shared_file_size = 0;
    uint16_t shared_file_full_threshold = 0;
    uint16_t shared_file_scale_rate = 0;
    uint32_t chainbase_flags = 0;
    bool do_validate_invariants = false;
    bool benchmark_is_enabled = false;
    fc::variant database_cfg;
    bool replay_in_memory = false;
    std::vector< std::string > replay_memory_indices{};
    bool load_snapshot = false;

    // The following fields are only used on reindexing
    uint32_t stop_replay_at = 0;
    bool force_replay = false;
    bool validate_during_replay = false;
  };

  /**
    *   @class database
    *   @brief tracks the blockchain state in an extensible manner
    */
  class database : public chainbase::database
  {
      friend class database_impl;

    public:
      database( appbase::application& app );
      ~database();

      void set_block_writer( block_write_i* writer );

      enum transaction_status
      {
        TX_STATUS_NONE       = 0x00, //outside any transaction processing
        TX_STATUS_UNVERIFIED = 0x01, //new transaction from API or P2P
        TX_STATUS_PENDING    = 0x02, //transaction that was verified by the node and is now pending (or popped)
        TX_STATUS_BLOCK      = 0x08, //during block processing
        TX_STATUS_P2P_BLOCK  = TX_STATUS_BLOCK | TX_STATUS_UNVERIFIED, //while processing block from P2P
        TX_STATUS_GEN_BLOCK  = TX_STATUS_BLOCK | TX_STATUS_PENDING //while generating new block
      };

      // block coming from P2P is validated for the first time, also newly generated or even reapplied but after switching fork
      bool is_validating_block() const { return _current_tx_status == TX_STATUS_P2P_BLOCK; }
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
      // returns id of block while it is being processed (or default 0)
      block_id_type get_processed_block_id() const { return _currently_processing_block_id.value_or( block_id_type() ); }

      void set_tx_status(transaction_status s)
      {
        bool tx_status_condition = ( _current_tx_status != TX_STATUS_NONE );
        if( tx_status_condition )
        {
          wlog( "Nested tx processing: _current_tx_status==${cs}, incoming ${s}",
            ( "cs", (int)_current_tx_status )( "s", (int)s ) );
#ifdef USE_ALTERNATE_CHAIN_ID
          FC_ASSERT( not tx_status_condition,
            "Nested tx processing: _current_tx_status==${cs}, incoming ${s}",
            ( "cs", (int)_current_tx_status )( "s", (int)s ) );
#endif
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
        skip_undo_block             = 1 << 12, ///< used to skip undo db (during sync prior to last checkpoint and during reindex)
        skip_block_log              = 1 << 13  ///< used to skip block logging on reindex
      };

      /**
        * @brief Preliminary part of state database opening. Requires following call to open.
        *
        * Opens a database in the specified directory. If no initialized database is found the database
        * will be initialized with the default state.
        * 
        * Exists after independent irreversible storage is initiated (no index loaded).
        */
      void pre_open( const open_args& args );
      /**
        * @brief Main part of state database opening. Requires earlier call to pre_open.
        */
      void open( const open_args& args );

    private:

      uint32_t reindex_internal( const open_args& args, const std::shared_ptr<full_block_type>& full_block, hive::chain::blockchain_worker_thread_pool& thread_pool );
      void remove_expired_governance_votes();

      //Remove proposal votes for accounts that declined voting rights during HF28.
      void remove_proposal_votes_for_accounts_without_voting_rights();

      /// Allows to load all data being independent to the persistent storage held in shared memory file.
      void initialize_state_independent_data(const open_args& args);

      void begin_type_register_process(util::abstract_type_registrar& r);

      void verify_match_of_state_objects_definitions_from_shm();
      void verify_match_of_blockchain_configuration();

    public:
      /// Allows to load all required initial data from persistent storage held in shared memory file. Must be used directly after opening a database, but also after loading a snapshot.
      void load_state_initial_data(const open_args& args);

      /**
        * @brief wipe Delete database from disk.
        *
        * Will close the database before wiping. Database will be closed when this function returns.
        */
      void wipe(const fc::path& shared_mem_dir);
       /// Watch out for superclass' variant of close.
      void close() /*override*/;

      //////////////////// db_block.cpp ////////////////////

    public:
      bool          is_known_transaction( const transaction_id_type& id, bool ignore_pending = false )const;
      fc::sha256    get_pow_target()const;
      uint32_t      get_pow_summary_target()const;

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

      const comment_object*  find_comment( comment_id_type comment_id )const;

      comment get_comment( const account_id_type& author, const shared_string& permlink )const;
      comment get_comment( const account_name_type& author, const shared_string& permlink )const;

      comment find_comment( const account_id_type& author, const shared_string& permlink )const;
      comment find_comment( const account_name_type& author, const shared_string& permlink )const;

      comment get_comment( const account_id_type& author, const string& permlink )const;
      comment get_comment( const account_name_type& author, const string& permlink )const;

      comment find_comment( const account_id_type& author, const string& permlink )const;
      comment find_comment( const account_name_type& author, const string& permlink )const;

      const escrow_object&   get_escrow(  const account_name_type& name, uint32_t escrow_id )const;
      const escrow_object*   find_escrow( const account_name_type& name, uint32_t escrow_id )const;

      const limit_order_object& get_limit_order(  const account_name_type& owner, uint32_t id )const;
      const limit_order_object* find_limit_order( const account_name_type& owner, uint32_t id )const;

      const savings_withdraw_object& get_savings_withdraw(  const account_name_type& owner, uint32_t request_id )const;
      const savings_withdraw_object* find_savings_withdraw( const account_name_type& owner, uint32_t request_id )const;

      const dynamic_global_property_object&  get_dynamic_global_properties()const;
      uint32_t                               get_node_skip_flags()const;
      void                                   set_node_skip_flags( uint32_t skip_flags );
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

      void max_bandwidth_per_share()const;

      /**
        *  Calculate the percent of block production slots that were missed in the
        *  past 128 blocks, not including the current block.
        */
      uint32_t witness_participation_rate()const;

      bool is_fast_confirm_transaction( const std::shared_ptr<full_transaction_type>& full_transaction) ;
      using switch_forks_t = std::function< std::optional< uint32_t >( std::shared_ptr<full_block_type>, uint32_t ) >;
      void process_fast_confirm_transaction(const std::shared_ptr<full_transaction_type>& full_transaction, switch_forks_t sf);
      void process_non_fast_confirm_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip = skip_nothing );
      void _push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction );

      void pop_block();
      /// Returns number of block on head after popping.
      uint32_t pop_block_extended( const block_id_type end_block );
      void clear_pending();

      /**
        *  This method is used to track applied operations during the evaluation of a block, these
        *  operations should include any operation actually included in a transaction as well
        *  as any implied/virtual operations that resulted, such as filling an order.
        *  The applied operations are cleared after post_apply_operation.
        */
      void notify_pre_apply_operation( const operation_notification& note );
      void notify_post_apply_operation( const operation_notification& note );

      /// Fill operation notification with current database context (block, trx, timestamp, etc.)
      /// and optionally increment the operation counter (for pre-push virtual operations)
      void fill_operation_notification( operation_notification& note, bool increment_op_counter = false );

      void notify_pre_apply_block( const block_notification& note );
      void notify_post_apply_block( const block_notification& note );
      void notify_fail_apply_block( const block_notification& note );
      void notify_irreversible_block( uint32_t block_num );
      void notify_switch_fork( uint32_t block_num );
      void notify_pre_apply_transaction( const transaction_notification& note );
      void notify_post_apply_transaction( const transaction_notification& note );
      void notify_pre_apply_custom_operation( const custom_operation_notification& note );
      void notify_post_apply_custom_operation( const custom_operation_notification& note );
      void notify_finish_push_block( const block_notification& note );
      void notify_flush();

      using apply_operation_handler_t = std::function< void(const operation_notification&) >;
      using apply_transaction_handler_t = std::function< void(const transaction_notification&) >;
      using apply_block_handler_t = std::function< void(const block_notification&) >;
      using push_block_handler_t = std::function< void(const block_notification&) >;
      using apply_custom_operation_handler_t = std::function< void(const custom_operation_notification&) >;
      using irreversible_block_handler_t = std::function< void(uint32_t) >;
      using flush_handler_t = std::function< void() >;
      using switch_fork_handler_t = std::function< void(uint32_t) >;
      using reindex_handler_t = std::function< void(const reindex_notification&) >;
      using prepare_snapshot_handler_t = std::function < void(const database&, const database::abstract_index_cntr_t&)>;
      using prepare_snapshot_data_supplement_handler_t = std::function < void(const prepare_snapshot_supplement_notification&) >;
      using load_snapshot_data_supplement_handler_t = std::function < void(const load_snapshot_supplement_notification&) >;
      using comment_reward_notification_handler_t = std::function < void(const comment_reward_notification&) >;
      using end_of_syncing_notification_handler_t = std::function < void(void) >;
      using wipe_notification_handler_t = std::function < void(void) >;

      void notify_prepare_snapshot_data_supplement(const prepare_snapshot_supplement_notification& n);
      void notify_load_snapshot_data_supplement(const load_snapshot_supplement_notification& n);
      void notify_comment_reward(const comment_reward_notification& note);
      void notify_end_of_syncing();
      void notify_wipe();
      void notify_pre_reindex(const reindex_notification& note);
      void notify_post_reindex(const reindex_notification& note);

    public:

      using signal_connection_ptr = hive::utilities::signal_connection_ptr;

      signal_connection_ptr add_pre_apply_operation_handler       ( const apply_operation_handler_t&           func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_post_apply_operation_handler      ( const apply_operation_handler_t&           func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_pre_apply_transaction_handler     ( const apply_transaction_handler_t&         func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_post_apply_transaction_handler    ( const apply_transaction_handler_t&         func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_pre_apply_block_handler           ( const apply_block_handler_t&               func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_post_apply_block_handler          ( const apply_block_handler_t&               func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_fail_apply_block_handler          ( const apply_block_handler_t&               func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_irreversible_block_handler        ( const irreversible_block_handler_t&        func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_switch_fork_handler               ( const switch_fork_handler_t&        func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_pre_reindex_handler               ( const reindex_handler_t&                   func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_post_reindex_handler              ( const reindex_handler_t&                   func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_pre_apply_custom_operation_handler ( const apply_custom_operation_handler_t&    func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_post_apply_custom_operation_handler( const apply_custom_operation_handler_t&    func, const abstract_plugin& plugin, int32_t group = -1 );
      signal_connection_ptr add_finish_push_block_handler          ( const push_block_handler_t&                func, const abstract_plugin& plugin, int32_t group = -1 );

      signal_connection_ptr add_prepare_snapshot_handler          (const prepare_snapshot_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);
      /// <summary>
      ///  All plugins storing data in different way than chainbase::generic_index (wrapping
      ///  a multi_index) should register to this handler to add its own data to the prepared snapshot.
      /// </summary>
      /// <param name="func">function to be called on notification</param>
      /// <param name="plugin">the plugin be registering its handler</param>
      /// <param name="group"></param>
      /// <returns></returns>
      signal_connection_ptr add_snapshot_supplement_handler       (const prepare_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);
      /// <summary>
      ///  All plugins storing data in different way than chainbase::generic_index (wrapping
      ///  a multi_index) should register to this handler to load its own data from the loaded snapshot.
      /// </summary>
      /// <param name="func"></param>
      /// <param name="plugin"></param>
      /// <param name="group"></param>
      /// <returns></returns>
      signal_connection_ptr add_snapshot_supplement_handler       (const load_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);

      signal_connection_ptr add_comment_reward_handler            (const comment_reward_notification_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);

      signal_connection_ptr add_end_of_syncing_handler            (const end_of_syncing_notification_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);

      signal_connection_ptr add_wipe_handler                      (const wipe_notification_handler_t& func, const abstract_plugin& plugin, int32_t group = -1);
      signal_connection_ptr add_flush_handler                     ( const flush_handler_t& func, const abstract_plugin& plugin, int32_t group = -1 );

      /// Register a callback for plugin index initialization (called during initialize_indexes)
      signal_connection_ptr add_plugin_index_handler( const std::function<void()>& func );

      //////////////////// db_witness_schedule.cpp ////////////////////

      void flush_to_all_storages();

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
      std::pair< HBD_asset, HIVE_asset > create_hbd( const account_object& to_account, const HIVE_asset& hive, bool to_reward_balance=false );

      using Before = std::function< void( const VEST_asset& ) >;
      VEST_asset adjust_account_vesting_balance( const account_object& to_account, const HIVE_asset& liquid, bool to_reward_balance, Before&& before_vesting_callback );

      VEST_asset create_vesting( const account_object& to_account, const HIVE_asset& liquid, bool to_reward_balance=false );

      void adjust_liquidity_reward( const account_object& owner, const asset& volume, bool is_hbd );

      void adjust_balance( const account_object& a, const asset& delta );
      void adjust_balance( const account_object& a, const HIVE_asset& delta );
      void adjust_balance( const account_object& a, const HBD_asset& delta );
      void adjust_balance( const account_name_type& name, const asset& delta ) { adjust_balance( get_account( name ), delta ); }
      void adjust_balance( const account_name_type& name, const HIVE_asset& delta ) { adjust_balance( get_account( name ), delta ); }
      void adjust_balance( const account_name_type& name, const HBD_asset& delta ) { adjust_balance( get_account( name ), delta ); }

      void adjust_savings_balance( const account_object& a, const asset& delta );
      void adjust_savings_balance( const account_object& a, const HIVE_asset& delta );
      void adjust_savings_balance( const account_object& a, const HBD_asset& delta );

      void adjust_reward_balance( const account_object& a, const asset& value_delta, const asset& share_delta = asset(0,VESTS_SYMBOL) );
      void adjust_reward_balance( const account_object& a, const HIVE_asset& value_delta );
      void adjust_reward_balance( const account_object& a, const HBD_asset& value_delta );
      void adjust_reward_balance( const account_object& a, const HIVE_asset& value_delta, const VEST_asset& share_delta );
      void adjust_reward_balance( const account_name_type& name, const asset& value_delta, const asset& share_delta = asset(0,VESTS_SYMBOL) )
      {
        adjust_reward_balance( get_account( name ), value_delta, share_delta );
      }
      void adjust_reward_balance( const account_name_type& name, const HIVE_asset& value_delta )
      {
        adjust_reward_balance( get_account( name ), value_delta );
      }
      void adjust_reward_balance( const account_name_type& name, const HBD_asset& value_delta )
      {
        adjust_reward_balance( get_account( name ), value_delta );
      }
      void adjust_reward_balance( const account_name_type& name, const HIVE_asset& value_delta, const VEST_asset& share_delta )
      {
        adjust_reward_balance( get_account( name ), value_delta, share_delta );
      }

      void adjust_supply( const asset& delta, bool adjust_vesting = false );
      void adjust_supply( const HIVE_asset& delta, bool adjust_vesting = false );
      void adjust_supply( const HBD_asset& delta );
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
      HIVE_asset pay_curators( const comment_object& comment, const comment_cashout_object& comment_cashout, HIVE_asset& max_rewards );
      HIVE_asset cashout_comment_helper( util::comment_reward_context& ctx, const comment_object& comment,
        const comment_cashout_object& comment_cashout, const comment_cashout_ex_object* comment_cashout_ex,
        bool forward_curation_remainder = true );
      void process_comment_cashout();
      void process_funds();
      void process_conversions();
      void remove_pending_conversion_requests( const account_object& account );
      void get_convert_request_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& collateralized_count, uint64_t& convert_count ) const;
      void process_savings_withdraws();
      void process_subsidized_accounts();
      void account_recovery_processing();
      void expire_escrow_ratification();
      void remove_pending_escrows( const account_object& account, const account_name_type& account_name );
      void get_escrow_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& escrow_count ) const;
      void remove_pending_savings_withdraws( const account_object& account, const account_name_type& account_name );
      void get_savings_withdraw_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& withdrawal_count ) const;
      void get_limit_order_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& order_count ) const;
      void remove_pending_limit_orders( const account_object& account, const account_name_type& account_name );
      void process_decline_voting_rights();
      void update_median_feed();

      asset get_liquidity_reward()const;
      asset get_content_reward()const;
      asset get_producer_reward();
      asset get_curation_reward()const;
      HIVE_asset get_pow_reward()const;

      uint16_t get_curation_rewards_percent() const;

      share_type pay_reward_funds( const share_type& reward );

      void  pay_liquidity_reward();

      /**
        * Helper method to return the current HBD value of a given amount of
        * HIVE.  Return 0 HBD if there isn't a current_median_history
        */
      HBD_asset to_hbd( const HIVE_asset& hive )const;
      HIVE_asset to_hive( const HBD_asset& hbd )const;

      time_point_sec   head_block_time()const;
      uint32_t         head_block_num()const;
      block_id_type    head_block_id()const;

      uint32_t get_last_irreversible_block_num()const;
      void set_last_irreversible_block_num(uint32_t block_num);

      std::shared_ptr<full_block_type> get_last_irreversible_block_data() const;
      void set_last_irreversible_block_data(std::shared_ptr<full_block_type> new_block);

      const irreversible_block_data_type* get_last_irreversible_object() const;
      //////////////////// db_init.cpp ////////////////////

      void initialize_evaluators();
      void register_custom_operation_interpreter( std::shared_ptr< custom_operation_interpreter > interpreter );
      std::shared_ptr< custom_operation_interpreter > get_custom_json_evaluator( const custom_id_type& id );

      /// Reset the object graph in-memory
      void initialize_indexes();

      // Reset irreversible state (unaffected by undo)
      void initialize_irreversible_storage();

      // For snapshot plugin. Decoded types data hold by database will be erased after using.
      std::string get_current_decoded_types_data_json();

      void init_schema();
      void init_genesis();

      /** when popping a block, the transactions that were removed get cached here so they
        * can be reapplied at the proper time */
      std::deque<std::shared_ptr<full_transaction_type>>       _popped_tx;
      vector<std::shared_ptr<full_transaction_type>>           _pending_tx;
      size_t                                                   _pending_tx_size = 0;
      size_t                                                   _max_mempool_size = 0;
      // alternative for transaction_index holding ids of pending and popped transactions
      std::unordered_set<transaction_id_type>                  _pending_tx_index;
      // collection of custom operation impacted accounts (related to limit of 5 per user per block)
      std::unordered_map<account_name_type, uint8_t>           _pending_tx_custom_op_count;

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

      std::optional< std::pair< chainbase::database::undo_session_guard, chainbase::database::undo_session_guard >>& pending_transaction_session()
      {
        return _pending_tx_session;
      }

#ifdef IS_TEST_NET
      bool liquidity_rewards_enabled = true;
      bool skip_price_feed_limit_check = true;
      bool skip_transaction_delta_check = true;
      bool disable_low_mem_warning = true;
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

      void apply_block(const std::shared_ptr<full_block_type>& full_block, uint32_t skip = skip_nothing, const block_flow_control* block_ctrl = nullptr );
      void apply_block_extended(  const std::shared_ptr<full_block_type>& full_block,
                                  uint32_t skip = skip_nothing,
                                  const block_flow_control* block_ctrl = nullptr );
      struct node_status_t {
        uint32_t last_processed_block_num;
        fc::time_point_sec last_processed_block_time;
      };
      node_status_t get_node_status();

    private:
      struct irreversible_block_data_type *last_irreversible_object = nullptr;
      std::shared_ptr<full_block_type> cached_lib;

      std::optional< std::pair< chainbase::database::undo_session_guard, chainbase::database::undo_session_guard >> _pending_tx_session;

      void _apply_block(const std::shared_ptr<full_block_type>& full_block, const block_flow_control* block_ctrl = nullptr );
      void validate_transaction(const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip);
      void _apply_transaction( const std::shared_ptr<full_transaction_type>& trx );

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
      void update_witness_hardfork_version_votes( const hardfork_version& hardfork_version, const fc::time_point_sec& hardfork_time );
      void update_witness_schedule_for_elected( const witness_object& current_witness, const util::rd_dynamics_params& account_subsidy_rd );
      uint64_t validate_witness_votes_invariant() const;
      void update_witness_missed_blocks( const account_name_type& block_witness, uint32_t missed_blocks );
      uint32_t update_last_irreversible_block(std::optional<switch_forks_t> sf);
      void migrate_irreversible_state(uint32_t old_last_irreversible);
      void clear_expired_transactions();
      void clear_expired_orders();
      void clear_expired_delegations();
      void process_header_extensions( const signed_block& next_block );
      void process_genesis_accounts();

      void init_hardforks();
      void process_hardforks();
      void apply_hardfork( uint32_t hardfork );
      void apply_hardfork_12_comment_cashout_fix();
      void apply_hardfork_17_comment_cashout_fix();

      ///@}

    public:

      const transaction_id_type& get_current_trx() const
      {
        return _current_trx_id;
      }
      uint16_t get_current_op_in_trx() const
      {
        return _current_op_in_trx;
      }

      int16_t get_remove_threshold() const;
      void set_remove_threshold( int16_t val );

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

      template <typename T>
      void register_new_type()
      {
        util::type_registrar<T> r;
        begin_type_register_process(r);
      }

    public:
      resource_credits& rc();
      const resource_credits& rc() const;
    private:

      std::unique_ptr< database_impl > _my;

      hardfork_versions             _hardfork_versions;

      block_write_i*                _block_writer;


      transaction_status            _current_tx_status = TX_STATUS_NONE;
      transaction_id_type           _current_trx_id;
      uint32_t                      _current_block_num    = 0;
      int32_t                       _current_trx_in_block = 0;
      uint32_t                      _current_op_in_trx    = 0;

      const struct operation_notification* _current_applied_operation_info = nullptr;

      optional< block_id_type >     _currently_processing_block_id;

      node_property_object          _node_property_object;

      uint32_t                      _flush_blocks = 0;
      uint32_t                      _next_flush_block = 0;

      uint32_t                      _last_free_gb_printed = 0;

      uint16_t                      _shared_file_full_threshold = 0;
      uint16_t                      _shared_file_scale_rate = 0;

      bool                          snapshot_loaded = false;

      std::optional<time_point_sec> _current_timestamp;

      comments_handler_ptr          _comments_handler;

    public:

      time_point_sec get_current_timestamp() const
      {
        if( _current_timestamp )
          return *_current_timestamp;
        else
          return get_dynamic_global_properties().time;
      }

      void set_comments_handler( comments_handler_ptr obj )
      {
        _comments_handler = obj;
      }

      comments_handler& get_comments_handler() const
      {
        FC_ASSERT( _comments_handler );
        return *_comments_handler.get();
      }

    private:

      flat_map< custom_id_type, std::shared_ptr< custom_operation_interpreter > >   _custom_operation_interpreters;
      std::string                   _json_schema;

      util::advanced_benchmark_dumper  _benchmark_dumper;

      appbase::application& theApp;

    public:
      
      appbase::application& get_app()
      {
        return theApp;
      }
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
