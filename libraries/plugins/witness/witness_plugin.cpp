#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/impacted.hpp>

#include <hive/utilities/key_conversion.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/macros.hpp>
#include <fc/smart_ref_impl.hpp>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>


#define DISTANCE_CALC_PRECISION (10000)
#define BLOCK_PRODUCING_LAG_TIME (750)
#define BLOCK_PRODUCTION_LOOP_SLEEP_TIME (200000)
#define DEFAULT_WITNESS_PARTICIPATION (33)


namespace hive { namespace plugins { namespace witness {

using namespace hive::chain;

using std::string;
using std::vector;

namespace bpo = boost::program_options;


void new_chain_banner( const chain::database& db )
{
  std::cerr << "\n"
    "********************************\n"
    "*                              *\n"
    "*   ------- NEW CHAIN ------   *\n"
    "*   -   Welcome to Hive!   -   *\n"
    "*   ------------------------   *\n"
    "*                              *\n"
    "********************************\n"
    "\n";
  return;
}

namespace detail {

  class witness_plugin_impl {
  public:
    witness_plugin_impl( boost::asio::io_service& io ) :
      _timer(io),
      _chain_plugin( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >() ),
      _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ),
      _block_producer( std::make_shared< witness::block_producer >( _db ) )
      {}

    void on_post_apply_block( const chain::block_notification& note );
    void on_pre_apply_operation( const chain::operation_notification& note );

    void schedule_production_loop();
    block_production_condition::block_production_condition_enum block_production_loop();
    block_production_condition::block_production_condition_enum maybe_produce_block(fc::mutable_variant_object& capture);

    bool     _production_enabled              = false;
    bool     _is_p2p_enabled                  = false;
    uint32_t _required_witness_participation  = DEFAULT_WITNESS_PARTICIPATION * HIVE_1_PERCENT;
    uint32_t _production_skip_flags           = chain::database::skip_nothing;

    std::map< hive::protocol::public_key_type, fc::ecc::private_key > _private_keys;
    std::set< hive::protocol::account_name_type >                     _witnesses;
    boost::asio::deadline_timer                                        _timer;

    plugins::chain::chain_plugin& _chain_plugin;
    chain::database&              _db;
    boost::signals2::connection   _post_apply_block_conn;
    boost::signals2::connection   _pre_apply_operation_conn;

    std::shared_ptr< witness::block_producer >                         _block_producer;
    uint32_t _last_fast_confirmation_block_number = 0;

    std::atomic<bool> _enable_fast_confirm = true;
  };

  class witness_generate_block_flow_control final : public generate_block_flow_control
  {
  public:
    using generate_block_flow_control::generate_block_flow_control;
    virtual ~witness_generate_block_flow_control() = default;

    virtual void on_fork_db_insert() const override
    {
      appbase::app().get_plugin< hive::plugins::p2p::p2p_plugin >().broadcast_block( full_block );
      generate_block_flow_control::on_fork_db_insert();
    }
  };

  struct comment_options_extension_visitor
  {
    comment_options_extension_visitor( const comment_object& c, const database& db ) : _c( c ), _db( db ) {}

    typedef void result_type;

    const comment_object& _c;
    const database& _db;

#ifdef HIVE_ENABLE_SMT
    void operator()( const allowed_vote_assets& va) const
    {
      FC_TODO("To be implemented support for allowed_vote_assets");
    }
#endif

    void operator()( const comment_payout_beneficiaries& cpb )const
    {
      HIVE_ASSERT( cpb.beneficiaries.size() <= 8,
        plugin_exception,
        "Cannot specify more than 8 beneficiaries." );
    }
  };

  void check_memo( const string& memo, const chain::account_object& account, const account_authority_object& auth )
  {
    vector< public_key_type > keys;

    try
    {
      // Check if memo is a private key
      keys.push_back( fc::ecc::extended_private_key::from_base58( memo ).get_public_key() );
    }
    catch( fc::parse_error_exception& ) {}
    catch( fc::assert_exception& ) {}

    // Get possible keys if memo was an account password
    string owner_seed = account.name + "owner" + memo;
    auto owner_secret = fc::sha256::hash( owner_seed.c_str(), owner_seed.size() );
    keys.push_back( fc::ecc::private_key::regenerate( owner_secret ).get_public_key() );

    string active_seed = account.name + "active" + memo;
    auto active_secret = fc::sha256::hash( active_seed.c_str(), active_seed.size() );
    keys.push_back( fc::ecc::private_key::regenerate( active_secret ).get_public_key() );

    string posting_seed = account.name + "posting" + memo;
    auto posting_secret = fc::sha256::hash( posting_seed.c_str(), posting_seed.size() );
    keys.push_back( fc::ecc::private_key::regenerate( posting_secret ).get_public_key() );

    // Check keys against public keys in authorites
    for( auto& key_weight_pair : auth.owner.key_auths )
    {
      for( auto& key : keys )
        HIVE_ASSERT( key_weight_pair.first != key,  plugin_exception,
          "Detected private owner key in memo field. You should change your owner keys." );
    }

    for( auto& key_weight_pair : auth.active.key_auths )
    {
      for( auto& key : keys )
        HIVE_ASSERT( key_weight_pair.first != key,  plugin_exception,
          "Detected private active key in memo field. You should change your active keys." );
    }

    for( auto& key_weight_pair : auth.posting.key_auths )
    {
      for( auto& key : keys )
        HIVE_ASSERT( key_weight_pair.first != key,  plugin_exception,
          "Detected private posting key in memo field. You should change your posting keys." );
    }

    const auto& memo_key = account.memo_key;
    for( auto& key : keys )
      HIVE_ASSERT( memo_key != key,  plugin_exception,
        "Detected private memo key in memo field. You should change your memo key." );
  }

  struct operation_visitor
  {
    operation_visitor( chain::database& db ) : _db( db ) {}

    chain::database& _db;

    typedef void result_type;

    template< typename T >
    void operator()( const T& )const {}

    void limit_custom_op_count( const operation& op )const
    {
      flat_set< account_name_type > impacted;
      app::operation_get_impacted_accounts( op, impacted );

      for( const account_name_type& account : impacted )
      {
        // Possible alternative implementation:  Don't call find(), simply catch
        // the exception thrown by db.create() when violating uniqueness (std::logic_error).
        //
        // This alternative implementation isn't "idiomatic" (i.e. AFAICT no existing
        // code uses this approach).  However, it may improve performance.

        const witness_custom_op_object* coo = _db.find< witness_custom_op_object, by_account >( account );

        if( !coo )
        {
          _db.create< witness_custom_op_object >( [&]( witness_custom_op_object& o )
          {
            o.account = account;
            o.count = 1;
          } );
        }
        else
        {
          HIVE_ASSERT( coo->count < WITNESS_CUSTOM_OP_BLOCK_LIMIT, plugin_exception,
            "Account ${a} already submitted ${n} custom json operation(s) this block.",
            ( "a", account )( "n", WITNESS_CUSTOM_OP_BLOCK_LIMIT ) );

          _db.modify( *coo, [&]( witness_custom_op_object& o )
          {
            o.count++;
          } );
        }
      }
    }

    void operator()( const custom_operation& o )const
    {
      limit_custom_op_count( o );
    }

    void operator()( const custom_json_operation& o )const
    {
      limit_custom_op_count( o );
    }

    void operator()( const custom_binary_operation& o )const
    {
      limit_custom_op_count( o );
    }

    void operator()( const comment_options_operation& o )const
    {
      const auto& comment = _db.get_comment( o.author, o.permlink );

      comment_options_extension_visitor v( comment, _db );

      for( auto& e : o.extensions )
      {
        e.visit( v );
      }
    }

    void operator()( const comment_operation& o )const
    {
      if( o.parent_author != HIVE_ROOT_POST_PARENT )
      {
        const auto& parent = _db.find_comment( o.parent_author, o.parent_permlink );

        if( parent != nullptr )
        HIVE_ASSERT( parent->get_depth() < HIVE_SOFT_MAX_COMMENT_DEPTH,
          plugin_exception,
          "Comment is nested ${x} posts deep, maximum depth is ${y}.", ("x",parent->get_depth())("y",HIVE_SOFT_MAX_COMMENT_DEPTH) );
      }
    }

    void operator()( const transfer_operation& o )const
    {
      if( o.memo.length() > 0 )
        check_memo( o.memo,
                _db.get< chain::account_object, chain::by_name >( o.from ),
                _db.get< account_authority_object, chain::by_account >( o.from ) );
    }

    void operator()( const transfer_to_savings_operation& o )const
    {
      if( o.memo.length() > 0 )
        check_memo( o.memo,
                _db.get< chain::account_object, chain::by_name >( o.from ),
                _db.get< account_authority_object, chain::by_account >( o.from ) );
    }

    void operator()( const transfer_from_savings_operation& o )const
    {
      if( o.memo.length() > 0 )
        check_memo( o.memo,
                _db.get< chain::account_object, chain::by_name >( o.from ),
                _db.get< account_authority_object, chain::by_account >( o.from ) );
    }
  };

  void witness_plugin_impl::on_pre_apply_operation( const chain::operation_notification& note )
  {
    if( _db.is_in_control() )
    {
      note.op.visit( operation_visitor( _db ) );
    }
  }

  void witness_plugin_impl::on_post_apply_block(const block_notification& note)
  {
    //note that we can't use clear on mutable version of this index because it bypasses undo sessions
    const auto& idx = _db.get_index<witness_custom_op_index>().indices().get<by_id>();
    while (true)
    {
      auto it = idx.begin();
      if (it == idx.end())
        break;
      _db.remove(*it);
    }

    // Broadcast a transaction to let the other witnesses know we've accepted this block for fast 
    // confirmation.
    // I think it's called multiple times during a fork switch, which isn't what we want, so
    // only generate this transaction if our head block number has increased
    //
    // TODO: we probably shouldn't broadcast anything if our head block is older than some threshold
    //       so we don't spam the network if we fall behind and are catching back up
    if (_db.has_hardfork(HIVE_HARDFORK_1_26_FAST_CONFIRMATION) && 
        note.block_num > _last_fast_confirmation_block_number)
    {
      std::set<account_name_type> scheduled_witnesses;
      const witness_schedule_object& wso_for_irreversibility = _db.get_witness_schedule_object_for_irreversibility();
      std::copy(wso_for_irreversibility.current_shuffled_witnesses.begin(), 
                wso_for_irreversibility.current_shuffled_witnesses.begin() + wso_for_irreversibility.num_scheduled_witnesses,
                std::inserter(scheduled_witnesses, scheduled_witnesses.end()));
      //ddump((scheduled_witnesses));

      for (const account_name_type& witness_name : _witnesses)
      {
        // dlog("In on_post_apply_block(), checking witness ${witness_name}", (witness_name));
        if (witness_name != note.full_block->get_block().witness && 
            scheduled_witnesses.find(witness_name) != scheduled_witnesses.end())
          try
          {
            chain::public_key_type scheduled_key;
            try
            {
              scheduled_key = _db.get<chain::witness_object, chain::by_name>(witness_name).signing_key;
            }
            catch (const fc::exception& e)
            {
              elog("unable to get witness's signing key for witness ${witness_name}: ${e}", (witness_name)(e));
            }
            auto private_key_itr = _private_keys.find(scheduled_key);
            if (private_key_itr != _private_keys.end())
            {
              // we're on the schedule and we have the keys required to generate the fast confirm op.
              if (_enable_fast_confirm.load(std::memory_order_relaxed))
              {
                witness_block_approve_operation op;
                op.witness = witness_name;
                op.block_id = note.block_id;

                signed_transaction tx;
                uint32_t last_irreversible_block = _db.get_last_irreversible_block_num();
                const block_id_type reference_block_id = last_irreversible_block ? _db.get_block_id_for_num(last_irreversible_block) : _db.head_block_id();
                tx.set_reference_block(reference_block_id);
                tx.set_expiration(_db.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
                tx.operations.push_back( op );
                tx.sign(private_key_itr->second, _db.get_chain_id(), fc::ecc::fc_canonical);

                ilog("Broadcasting fast-confirm transaction for ${witness_name}, block #${block_num}", (witness_name)("block_num", note.block_num));
                uint32_t skip = _db.get_node_properties().skip_flags;

                std::shared_ptr<full_transaction_type> full_transaction = full_transaction_type::create_from_signed_transaction(tx, hive::protocol::serialization_mode_controller::get_current_pack(), false);
                _db.push_transaction(full_transaction, skip);
                appbase::app().get_plugin<hive::plugins::p2p::p2p_plugin>().broadcast_transaction(full_transaction);
              }
              else
              {
                ilog("Not broadcasting fast-confirm transaction for ${witness_name}, block #${block_num}, because fast-confirm is disabled",
                     (witness_name)("block_num", note.block_num));
              }
            }
          }
          catch (const fc::exception& e)
          {
            elog("Failed to broadcast fast-confirmation transaction for witness ${witness_name}: ${e}", (witness_name)(e));
          }
      }

      _last_fast_confirmation_block_number = note.block_num;
    }
  }

  void witness_plugin_impl::schedule_production_loop() {
    // Sleep for 200ms, before checking the block production
    fc::time_point now = fc::time_point::now();
    int64_t time_to_sleep = BLOCK_PRODUCTION_LOOP_SLEEP_TIME - (now.time_since_epoch().count() % BLOCK_PRODUCTION_LOOP_SLEEP_TIME);
    if (time_to_sleep < 50000) // we must sleep for at least 50ms
        time_to_sleep += BLOCK_PRODUCTION_LOOP_SLEEP_TIME;

    _timer.expires_from_now( boost::posix_time::microseconds( time_to_sleep ) );
    _timer.async_wait( boost::bind( &witness_plugin_impl::block_production_loop, this ) );
  }

  block_production_condition::block_production_condition_enum witness_plugin_impl::block_production_loop()
  {
    if( fc::time_point::now() < fc::time_point(HIVE_GENESIS_TIME) )
    {
      wlog( "waiting until genesis time to produce block: ${t}", ("t",HIVE_GENESIS_TIME) );
      schedule_production_loop();
      return block_production_condition::wait_for_genesis;
    }

    block_production_condition::block_production_condition_enum result;
    fc::mutable_variant_object capture;
    try
    {
      result = maybe_produce_block(capture);
    }
    catch( const fc::canceled_exception& )
    {
      //We're trying to exit. Go ahead and let this one out.
      throw;
    }
    catch( const chain::unknown_hardfork_exception& e )
    {
      // Hit a hardfork that the current node know nothing about, stop production and inform user
      elog( "${e}\nNode may be out of date...", ("e", e.to_detail_string()) );
      throw;
    }
    catch( const fc::exception& e )
    {
      elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
      result = block_production_condition::exception_producing_block;
    }

    switch(result)
    {
      case block_production_condition::produced:
        ilog("Generated block #${n} with timestamp ${t} at time ${c}", ("n", capture["n"])("t", capture["t"])("c", capture["c"]));
        
        break;
      case block_production_condition::not_synced:
  //         ilog("Not producing block because production is disabled until we receive a recent block (see: --enable-stale-production)");
        break;
      case block_production_condition::not_my_turn:
  //         ilog("Not producing block because it isn't my turn");
        break;
      case block_production_condition::not_time_yet:
  //         ilog("Not producing block because slot has not yet arrived");
        break;
      case block_production_condition::no_private_key:
        ilog("Not producing block because I don't have the private key for ${scheduled_key}", ("scheduled_key", capture["scheduled_key"]) );
        break;
      case block_production_condition::low_participation:
        elog("Not producing block because node appears to be on a minority fork with only ${pct}% witness participation", ("pct", capture["pct"]/HIVE_1_PERCENT) );
        break;
      case block_production_condition::lag:
        elog("Not producing block because node didn't wake up within ${t}ms of the slot time.", ("t", BLOCK_PRODUCING_LAG_TIME));
        break;
      case block_production_condition::consecutive:
        elog("Not producing block because the last block was generated by the same witness.\nThis node is probably disconnected from the network so block production has been disabled.\nDisable this check with --allow-consecutive option.");
        break;
      case block_production_condition::exception_producing_block:
        elog( "exception producing block" );
        break;
      case block_production_condition::wait_for_genesis:
        break;
    }

    schedule_production_loop();
    return result;
  }

  block_production_condition::block_production_condition_enum witness_plugin_impl::maybe_produce_block(fc::mutable_variant_object& capture)
  {
    fc::time_point now_fine = fc::time_point::now();
    fc::time_point_sec now = now_fine + fc::microseconds( 500000 );

    // If the next block production opportunity is in the present or future, we're synced.
    if( !_production_enabled )
    {
      if( _db.get_slot_time(1) >= now )
        _production_enabled = true;
      else
        return block_production_condition::not_synced;
    }

    // is anyone scheduled to produce now or one second in the future?
    uint32_t slot = _db.get_slot_at_time( now );
    if( slot == 0 )
    {
      capture("next_time", _db.get_slot_time(1));
      return block_production_condition::not_time_yet;
    }

    //
    // this assert should not fail, because now <= db.head_block_time()
    // should have resulted in slot == 0.
    //
    // if this assert triggers, there is a serious bug in get_slot_at_time()
    // which would result in allowing a later block to have a timestamp
    // less than or equal to the previous block
    //
    assert( now > _db.head_block_time() );

    chain::account_name_type scheduled_witness = _db.get_scheduled_witness( slot );
    // we must control the witness scheduled to produce the next block.
    if( _witnesses.find( scheduled_witness ) == _witnesses.end() )
    {
      capture("scheduled_witness", scheduled_witness);
      return block_production_condition::not_my_turn;
    }

    fc::time_point_sec scheduled_time = _db.get_slot_time( slot );
    chain::public_key_type scheduled_key = _db.get< chain::witness_object, chain::by_name >(scheduled_witness).signing_key;
    auto private_key_itr = _private_keys.find( scheduled_key );

    if( private_key_itr == _private_keys.end() )
    {
      capture("scheduled_witness", scheduled_witness);
      capture("scheduled_key", scheduled_key);
      return block_production_condition::no_private_key;
    }

    uint32_t prate = _db.witness_participation_rate();
    if( prate < _required_witness_participation )
    {
      capture("pct", uint32_t(100*uint64_t(prate) / HIVE_1_PERCENT));
      return block_production_condition::low_participation;
    }

    if( llabs((scheduled_time - now).count()) > fc::milliseconds( BLOCK_PRODUCING_LAG_TIME ).count() )
    {
      capture("scheduled_time", scheduled_time)("now", now);
      return block_production_condition::lag;
    }

    auto generate_block_ctrl = std::make_shared< witness_generate_block_flow_control >( scheduled_time,
      scheduled_witness, private_key_itr->second, _production_skip_flags );
    _chain_plugin.generate_block( generate_block_ctrl );
    const std::shared_ptr<full_block_type>& full_block = generate_block_ctrl->get_full_block();
    capture("n", full_block->get_block_num())("t", full_block->get_block_header().timestamp)("c", now);

    //appbase::app().get_plugin<hive::plugins::p2p::p2p_plugin>().broadcast_block(full_block);
    // above is executed by generate_block_ctrl after block is inserted to fork-db, but the thread is kept waiting
    // until block is reapplied, or the same block would try to be produced again
    return block_production_condition::produced;
  }
} // detail


witness_plugin::witness_plugin() {}
witness_plugin::~witness_plugin() {}

void witness_plugin::enable_fast_confirm()
{
  my->_enable_fast_confirm.store(true, std::memory_order_relaxed);
}

void witness_plugin::disable_fast_confirm()
{
  my->_enable_fast_confirm.store(false, std::memory_order_relaxed);
}

bool witness_plugin::is_fast_confirm_enabled() const
{
  return my->_enable_fast_confirm.load(std::memory_order_relaxed);
}

void witness_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg)
{
  string witness_id_example = "initwitness";
  cfg.add_options()
      ("enable-stale-production", bpo::value<bool>()->default_value( false ), "Enable block production, even if the chain is stale.")
      ("required-participation", bpo::value< uint32_t >()->default_value( DEFAULT_WITNESS_PARTICIPATION ), "Percent of witnesses (0-99) that must be participating in order to produce blocks")
      ("witness,w", bpo::value<vector<string>>()->composing()->multitoken(),
        ("name of witness controlled by this node (e.g. " + witness_id_example + " )" ).c_str() )
      ("private-key", bpo::value<vector<string>>()->composing()->multitoken(), "WIF PRIVATE KEY to be used by one or more witnesses or miners" )
      ("witness-skip-enforce-bandwidth", bpo::value<bool>()->default_value( true ), "Skip enforcing bandwidth restrictions. Default is true in favor of rc_plugin." )
      ;
  cli.add_options()
      ("enable-stale-production", bpo::bool_switch()->default_value( false ), "Enable block production, even if the chain is stale.")
      ("witness-skip-enforce-bandwidth", bpo::bool_switch()->default_value( true ), "Skip enforcing bandwidth restrictions. Default is true in favor of rc_plugin." )
      ;
}

void witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
  ilog( "Initializing witness plugin" );
  my = std::make_unique< detail::witness_plugin_impl >( appbase::app().get_io_service() );

  my->_chain_plugin.register_block_generator( get_name(), my->_block_producer );

  HIVE_LOAD_VALUE_SET( options, "witness", my->_witnesses, hive::protocol::account_name_type )

  if( options.count("private-key") )
  {
    const std::vector<std::string> keys = options["private-key"].as<std::vector<std::string>>();
    for (const std::string& wif_key : keys )
    {
      fc::optional<fc::ecc::private_key> private_key = hive::utilities::wif_to_key(wif_key);
      FC_ASSERT( private_key.valid(), "unable to parse private key" );
      my->_private_keys[private_key->get_public_key()] = *private_key;
    }
  }

  my->_production_enabled = options.at( "enable-stale-production" ).as< bool >();
  if (my->_production_enabled)
    wlog("warning: stale production is enabled, make sure you know what you are doing.");

  if( my->_witnesses.size() > 0 )
  {
    // It is safe to access rc plugin here because of APPBASE_REQUIRES_PLUGIN
    FC_ASSERT( !appbase::app().get_plugin< rc::rc_plugin >().get_rc_plugin_skip_flags().skip_reject_not_enough_rc,
      "rc-skip-reject-not-enough-rc=false is required to produce blocks" );
  }

  if( options.count( "required-participation" ) )
  {
    my->_required_witness_participation = HIVE_1_PERCENT * options.at( "required-participation" ).as< uint32_t >();
  }
  if ( my->_required_witness_participation < DEFAULT_WITNESS_PARTICIPATION * HIVE_1_PERCENT)
    wlog("warning: required witness participation=${required_witness_participation}, normally this should be set to ${default_witness_participation}",("required_witness_participation",my->_required_witness_participation / HIVE_1_PERCENT)("default_witness_participation",DEFAULT_WITNESS_PARTICIPATION));

  my->_post_apply_block_conn = my->_db.add_post_apply_block_handler(
    [&]( const chain::block_notification& note ){ my->on_post_apply_block( note ); }, *this, 0 );
  my->_pre_apply_operation_conn = my->_db.add_pre_apply_operation_handler(
    [&]( const chain::operation_notification& note ){ my->on_pre_apply_operation( note ); }, *this, 0);

  //if a producing witness, allow up to 1/3 of the block interval for writing blocks/transactions (2x a normal node)
  if( my->_witnesses.size() && my->_private_keys.size() )
    my->_chain_plugin.set_write_lock_hold_time( HIVE_BLOCK_INTERVAL * 1000 / 3 ); // units = milliseconds

  HIVE_ADD_PLUGIN_INDEX(my->_db, witness_custom_op_index);

} FC_LOG_AND_RETHROW() }

void witness_plugin::plugin_startup()
{ try {
  ilog("witness plugin:  plugin_startup() begin" );
  auto& _chain_plugin = appbase::app().get_plugin< hive::plugins::chain::chain_plugin >();
  my->_is_p2p_enabled = _chain_plugin.is_p2p_enabled();
  chain::database& d  = _chain_plugin.db();

  if( !my->_is_p2p_enabled )
  {
    ilog("Witness plugin is not enabled, beause P2P plugin is disabled...");
    return;
  }

  if( !my->_witnesses.empty() )
  {
    ilog( "Launching block production for ${n} witnesses.", ("n", my->_witnesses.size()) );
    appbase::app().get_plugin< hive::plugins::p2p::p2p_plugin >().set_block_production( true );
    if( my->_production_enabled )
    {
      if( d.head_block_num() == 0 )
        new_chain_banner( d );
      my->_production_skip_flags |= chain::database::skip_undo_history_check;
    }
    my->schedule_production_loop();
  } else
    elog("No witnesses configured! Please add witness IDs and private keys to configuration.");
  ilog("witness plugin:  plugin_startup() end");
  } FC_CAPTURE_AND_RETHROW() }

void witness_plugin::plugin_shutdown()
{
  try
  {
    if( !my->_is_p2p_enabled )
    {
      ilog("Witness plugin is not enabled, beause P2P plugin is disabled...");
      return;
    }

    chain::util::disconnect_signal( my->_post_apply_block_conn );
    chain::util::disconnect_signal( my->_pre_apply_operation_conn );

    my->_timer.cancel();
  }
  catch(fc::exception& e)
  {
    edump( (e.to_detail_string()) );
  }
}

} } } // hive::plugins::witness
