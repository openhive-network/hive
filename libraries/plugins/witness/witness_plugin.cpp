#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/protocol/transaction_util.hpp>

#include <fc/io/json.hpp>
#include <fc/macros.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/value_set.hpp>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>


#define DISTANCE_CALC_PRECISION (10000)
#define BLOCK_PRODUCING_LAG_TIME (750)
#define BLOCK_PRODUCTION_LOOP_SLEEP_TIME (200000)
#define DEFAULT_WITNESS_PARTICIPATION (33)


namespace hive { namespace plugins { namespace witness {

using namespace hive::chain;
using namespace hive::protocol;

using std::string;
using std::vector;

namespace bpo = boost::program_options;


void new_chain_banner( const chain::database& db )
{
  ilog("\n"
    "********************************\n"
    "*                              *\n"
    "*   ------- NEW CHAIN ------   *\n"
    "*   -   Welcome to Hive!   -   *\n"
    "*   ------------------------   *\n"
    "*                              *\n"
    "********************************\n");
  return;
}

namespace detail {

  struct produce_block_data_t {
    produce_block_data_t() :
      next_slot( 0 ),
      next_slot_time( HIVE_GENESIS_TIME + HIVE_BLOCK_INTERVAL ),
      scheduled_witness( "" ),
      scheduled_private_key{},
      condition( block_production_condition::not_my_turn ),
      produce_in_next_slot( false )
    {}

    uint32_t next_slot;
    fc::time_point_sec next_slot_time;
    chain::account_name_type scheduled_witness;
    fc::ecc::private_key scheduled_private_key;
    block_production_condition condition;
    bool produce_in_next_slot;
  };

  class witness_plugin_impl {
  public:
    witness_plugin_impl( boost::asio::io_service& io, appbase::application& app ) :
      _timer(io),
      _chain_plugin( app.get_plugin< hive::plugins::chain::chain_plugin >() ),
      _db( _chain_plugin.db() ),
      _block_reader( _chain_plugin.block_reader() ),
      _block_producer( std::make_shared< witness::block_producer >( _chain_plugin ) ),
      theApp( app )
      {}

    void on_post_apply_block( const chain::block_notification& note );
    void on_pre_apply_operation( const chain::operation_notification& note );
    void on_finish_push_block( const chain::block_notification& note );
    produce_block_data_t get_produce_block_data(uint32_t slot);

    void schedule_production_loop();
    block_production_condition block_production_loop(const boost::system::error_code&);
    block_production_condition maybe_produce_block(fc::mutable_variant_object& capture);

    bool     _production_enabled              = false;
    bool     _is_p2p_enabled                  = false;
    uint32_t _required_witness_participation  = DEFAULT_WITNESS_PARTICIPATION * HIVE_1_PERCENT;
    uint32_t _production_skip_flags           = chain::database::skip_nothing;

    std::map< hive::protocol::public_key_type, fc::ecc::private_key > _private_keys;
    std::set< hive::protocol::account_name_type >                     _witnesses;
    boost::asio::deadline_timer                                       _timer;

    plugins::chain::chain_plugin& _chain_plugin;
    chain::database&              _db;
    const chain::block_read_i&    _block_reader;
    boost::signals2::connection   _post_apply_block_conn;
    boost::signals2::connection   _pre_apply_operation_conn;
    boost::signals2::connection   _finish_push_block_conn;

    std::shared_ptr< witness::block_producer >                        _block_producer;
    uint32_t _last_fast_confirmation_block_number = 0;

    produce_block_data_t produce_block_data = {};
    std::mutex should_produce_block_mutex = {};

    std::atomic<bool> _enable_fast_confirm = true;

    appbase::application& theApp;
  };

  class witness_generate_block_flow_control final : public generate_block_flow_control
  {
  public:
    witness_generate_block_flow_control( const fc::time_point_sec _block_ts,
      const protocol::account_name_type& _wo, const fc::ecc::private_key& _key, uint32_t _skip,
      appbase::application& app )
    : generate_block_flow_control( _block_ts, _wo, _key, _skip ),
      p2p( app.get_plugin< hive::plugins::p2p::p2p_plugin >() ) {}
    virtual ~witness_generate_block_flow_control() = default;

    virtual void on_fork_db_insert() const override
    {
      p2p.broadcast_block( full_block );
      generate_block_flow_control::on_fork_db_insert();
    }

  private:
    hive::plugins::p2p::p2p_plugin& p2p;
  };

  void check_memo( const string& memo, const chain::account_object& account, const account_authority_object& auth )
  {
    vector< public_key_type > keys;

    collect_potential_keys( &keys, account.get_name(), memo );

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

    void operator()( const recurrent_transfer_operation& o )const
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

  void witness_plugin_impl::on_post_apply_block( const block_notification& note )
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
  }

  void witness_plugin_impl::on_finish_push_block( const block_notification& note )
  {
    // Broadcast a transaction to let the other witnesses know we've accepted this block for fast 
    // confirmation.
    // I think it's called multiple times during a fork switch, which isn't what we want, so
    // only generate this transaction if our head block number has increased
    if( _db.has_hardfork(HIVE_HARDFORK_1_26_FAST_CONFIRMATION) &&
        note.block_num > _last_fast_confirmation_block_number &&
        _production_enabled && _is_p2p_enabled &&
        fc::time_point::now() - note.get_block_timestamp() < HIVE_UP_TO_DATE_MARGIN__FAST_CONFIRM )
    {
      std::set<account_name_type> scheduled_witnesses;
      const witness_schedule_object& wso_for_irreversibility = _db.get_witness_schedule_object_for_irreversibility();
      std::copy(wso_for_irreversibility.current_shuffled_witnesses.begin(), 
                wso_for_irreversibility.current_shuffled_witnesses.begin() + wso_for_irreversibility.num_scheduled_witnesses,
                std::inserter(scheduled_witnesses, scheduled_witnesses.end()));
      //ddump((scheduled_witnesses));

      for (const account_name_type& witness_name : _witnesses)
      {
        // dlog("In on_finish_push_block(), checking witness ${witness_name}", (witness_name));
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

                transaction tx;
                uint32_t last_irreversible_block = _db.get_last_irreversible_block_num();
                const block_id_type reference_block_id = 
                  last_irreversible_block ?
                    _block_reader.find_block_id_for_num(last_irreversible_block) :
                    _db.head_block_id();
                tx.set_reference_block(reference_block_id);
                tx.set_expiration(_db.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION/2);
                tx.operations.push_back( op );

                full_transaction_ptr full_transaction = full_transaction_type::create_from_transaction( tx, hive::protocol::pack_type::hf26 );
                std::vector< hive::protocol::private_key_type > keys;
                keys.emplace_back( private_key_itr->second );
                full_transaction->sign_transaction( keys, _db.get_chain_id(), hive::protocol::pack_type::hf26 );

                //ilog("Broadcasting fast-confirm transaction for ${witness_name}, block #${block_num}", (witness_name)("block_num", note.block_num));
                uint32_t skip = _db.get_node_skip_flags();

                _chain_plugin.push_transaction(full_transaction, skip);
                theApp.get_plugin<hive::plugins::p2p::p2p_plugin>().broadcast_transaction(full_transaction);
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
    {
      produce_block_data_t data = get_produce_block_data(1);
      std::unique_lock g(should_produce_block_mutex);
      produce_block_data = std::move(data);
    }
  }

  produce_block_data_t witness_plugin_impl::get_produce_block_data(uint32_t slot)
  {
    const auto next_block_time = _db.get_slot_time( slot );
    chain::account_name_type scheduled_witness;
    fc::ecc::private_key private_key;

    // immediately invoked lambda returning block_production_condition
    block_production_condition condition = [&]()
    {
      if( !_production_enabled )
      {
        if( _db.get_slot_time( 1 ) >= fc::time_point::now() )
          _production_enabled = true;
        else
          return block_production_condition::not_synced;
      }

      // we must control the witness scheduled to produce the next block.
      scheduled_witness = _db.get_scheduled_witness( slot );
      if( _witnesses.find( scheduled_witness ) == _witnesses.end() )
        return block_production_condition::not_my_turn;

      chain::public_key_type scheduled_key = _db.get< chain::witness_object, chain::by_name >( scheduled_witness ).signing_key;
      auto private_key_itr = _private_keys.find( scheduled_key );
      if( private_key_itr == _private_keys.end() )
      {
        ilog( "Won't produce block because I don't have the private key for ${scheduled_key}", ( scheduled_key ) );
        return block_production_condition::no_private_key;
      }
      private_key = private_key_itr->second;

      uint32_t prate = _db.witness_participation_rate();
      if( prate < _required_witness_participation )
      {
        uint32_t pct = uint32_t( 100 * uint64_t( prate ) / HIVE_100_PERCENT );
        elog( "Won't produce block because node appears to be on a minority fork with only ${pct}% witness participation", ( pct ) );
        return block_production_condition::low_participation;
      }
      return block_production_condition::produced;
    }();

    produce_block_data_t produce_block_data;
    produce_block_data.next_slot = slot;
    produce_block_data.next_slot_time = next_block_time;
    produce_block_data.scheduled_witness = std::move(scheduled_witness);
    produce_block_data.scheduled_private_key = private_key;
    produce_block_data.condition = condition;
    produce_block_data.produce_in_next_slot = condition == block_production_condition::produced;
    return produce_block_data;
  }

  void witness_plugin_impl::schedule_production_loop()
  {
    // Sleep for 200ms, before checking the block production
    fc::time_point now = fc::time_point::now();
    int64_t time_to_sleep = BLOCK_PRODUCTION_LOOP_SLEEP_TIME - (now.time_since_epoch().count() % BLOCK_PRODUCTION_LOOP_SLEEP_TIME);
    if( time_to_sleep < 50000 ) // we must sleep for at least 50ms
      time_to_sleep += BLOCK_PRODUCTION_LOOP_SLEEP_TIME;

    using boost::placeholders::_1;
    _timer.expires_from_now( boost::posix_time::microseconds( time_to_sleep ) );
    _timer.async_wait( boost::bind( &witness_plugin_impl::block_production_loop, this, _1 ) );
  }

  block_production_condition witness_plugin_impl::block_production_loop(const boost::system::error_code& e)
  {
    if (e) return {};

    if( fc::time_point::now() < fc::time_point(HIVE_GENESIS_TIME) )
    {
      wlog( "waiting until genesis time to produce block: ${t}", ("t",HIVE_GENESIS_TIME) );
      schedule_production_loop();
      return block_production_condition::wait_for_genesis;
    }

    block_production_condition result;
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
  //      ilog("Not producing block because production is disabled until we receive a recent block (see: --enable-stale-production)");
        break;
      case block_production_condition::not_my_turn:
  //      ilog("Not producing block because it isn't my turn");
        break;
      case block_production_condition::not_time_yet:
  //      ilog("Not producing block because slot has not yet arrived");
        break;
      case block_production_condition::no_private_key:
        break;
      case block_production_condition::low_participation:
        break;
      case block_production_condition::lag:
        elog("Not producing block because node didn't wake up within ${t}ms of the slot time.", ("t", BLOCK_PRODUCING_LAG_TIME));
        break;
      case block_production_condition::exception_producing_block:
        elog( "exception producing block" );
        break;
      case block_production_condition::unknown:
        elog( "Not producing block because could not determine whether to produce it" );
        break;
      case block_production_condition::wait_for_genesis:
        break;
    }

    if( theApp.is_interrupt_request() )
      ilog( "ending block_production_loop" );
    else
      schedule_production_loop();
    return result;
  }

  block_production_condition witness_plugin_impl::maybe_produce_block(fc::mutable_variant_object& capture)
  {
    std::lock_guard g(should_produce_block_mutex);
    fc::time_point_sec now;
    now = fc::time_point::now() + fc::microseconds( 500000 );

    if( !_production_enabled )
      return block_production_condition::not_synced;

    if( produce_block_data.next_slot_time > now)
    {
      capture("next_time", produce_block_data.next_slot);
      return block_production_condition::not_time_yet;
    }

    const fc::microseconds lag = produce_block_data.next_slot_time - now;
    if( llabs(lag.count()) > fc::milliseconds( BLOCK_PRODUCING_LAG_TIME ).count() )
    {
      capture("scheduled_time", produce_block_data.next_slot_time)("now", now);
      if (lag.count() < 0)
      {
        try {
          _db.with_read_lock([&](){
            produce_block_data = get_produce_block_data(produce_block_data.next_slot+1);
          }, fc::milliseconds(200));
        }
        catch (const chainbase::lock_exception& e) {
          // Do nothing
        }
      }
      return block_production_condition::lag;
    }

    if (!produce_block_data.produce_in_next_slot)
      return produce_block_data.condition;

    const auto generate_block_ctrl = std::make_shared< witness_generate_block_flow_control >( produce_block_data.next_slot_time,
      produce_block_data.scheduled_witness, produce_block_data.scheduled_private_key, _production_skip_flags, theApp );
    _chain_plugin.generate_block( generate_block_ctrl );
    const std::shared_ptr<full_block_type>& full_block = generate_block_ctrl->get_full_block();
    capture("n", full_block->get_block_num())("t", full_block->get_block_header().timestamp)("c", now);

    produce_block_data.produce_in_next_slot = false;
    produce_block_data.condition = block_production_condition::unknown;

    //theApp.get_plugin<hive::plugins::p2p::p2p_plugin>().broadcast_block(full_block);
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

void witness_plugin::set_witnesses( const std::set< hive::protocol::account_name_type >& witnesses )
{
  my->_witnesses = witnesses;
}

void witness_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg)
{
  string witness_id_example = "initwitness";
  cfg.add_options()
    ( "enable-stale-production", bpo::value<bool>()->default_value( false ), "Enable block production, even if the chain is stale." )
    ( "required-participation", bpo::value< uint32_t >()->default_value( DEFAULT_WITNESS_PARTICIPATION ), "Percent of witnesses (0-99) that must be participating in order to produce blocks" )
    ( "witness,w", bpo::value<vector<string>>()->composing()->multitoken(),
      ( "name of witness controlled by this node (e.g. " + witness_id_example + " )" ).c_str() )
    ( "private-key", bpo::value<vector<string>>()->composing()->multitoken(), "WIF PRIVATE KEY to be used by one or more witnesses or miners" )
    ;
  cli.add_options()
    ( "enable-stale-production", bpo::bool_switch()->default_value( false ), "Enable block production, even if the chain is stale." )
    ;
}

void witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
  ilog( "Initializing witness plugin" );
  my = std::make_unique< detail::witness_plugin_impl >( get_app().get_io_service(), get_app() );

  my->_chain_plugin.register_block_generator( get_name(), my->_block_producer );

  fc::load_value_set<hive::protocol::account_name_type>( options, "witness", my->_witnesses );

  if( options.count("private-key") )
  {
    const std::vector<std::string> keys = options["private-key"].as<std::vector<std::string>>();
    for (const std::string& wif_key : keys )
    {
      fc::optional<fc::ecc::private_key> private_key = fc::ecc::private_key::wif_to_key(wif_key);
      FC_ASSERT( private_key.valid(), "unable to parse private key" );
      my->_private_keys[private_key->get_public_key()] = *private_key;
    }
  }

  my->_production_enabled = options.at( "enable-stale-production" ).as< bool >();
  if( my->_production_enabled )
    wlog( "warning: stale production is enabled, make sure you know what you are doing." );

  if( options.count( "required-participation" ) )
  {
    my->_required_witness_participation = HIVE_1_PERCENT * options.at( "required-participation" ).as< uint32_t >();
  }
  if( my->_required_witness_participation < DEFAULT_WITNESS_PARTICIPATION * HIVE_1_PERCENT )
    wlog( "warning: required witness participation=${required_witness_participation}, normally this should be set to ${default_witness_participation}",("required_witness_participation",my->_required_witness_participation / HIVE_1_PERCENT)("default_witness_participation",DEFAULT_WITNESS_PARTICIPATION) );

  my->_post_apply_block_conn = my->_db.add_post_apply_block_handler(
    [&]( const chain::block_notification& note ){ my->on_post_apply_block( note ); }, *this, 0 );
  my->_pre_apply_operation_conn = my->_db.add_pre_apply_operation_handler(
    [&]( const chain::operation_notification& note ){ my->on_pre_apply_operation( note ); }, *this, 0 );
  my->_finish_push_block_conn = my->_db.add_finish_push_block_handler(
    [&]( const chain::block_notification& note ){ my->on_finish_push_block( note ); }, *this, 0 );

  //if a producing witness, allow up to 1/3 of the block interval for writing blocks/transactions (2x a normal node)
  if( my->_witnesses.size() && my->_private_keys.size() )
    my->_chain_plugin.set_write_lock_hold_time( HIVE_BLOCK_INTERVAL * 1000 / 3 ); // units = milliseconds

  HIVE_ADD_PLUGIN_INDEX(my->_db, witness_custom_op_index);

} FC_LOG_AND_RETHROW() }

void witness_plugin::plugin_startup()
{ try {
  ilog( "witness plugin:  plugin_startup() begin" );
  my->_is_p2p_enabled = my->_chain_plugin.is_p2p_enabled();

  if( !my->_is_p2p_enabled )
  {
    ilog( "Witness plugin is not enabled, because P2P plugin is disabled..." );
    return;
  }

  if( !my->_witnesses.empty() )
  {
    ilog( "Launching block production for ${n} witnesses.", ( "n", my->_witnesses.size() ) );
    get_app().get_plugin< hive::plugins::p2p::p2p_plugin >().set_block_production( true );
    if( my->_production_enabled )
    {
      if( my->_db.head_block_num() == 0 )
        new_chain_banner( my->_db );
      my->_production_skip_flags |= chain::database::skip_undo_history_check;
    }
    fc::time_point now = fc::time_point::now();
    const uint32_t slot = my->_db.get_slot_at_time(now);
    my->produce_block_data = my->get_produce_block_data(slot);
    my->schedule_production_loop();
  }
  else
  {
    elog( "No witnesses configured! Please add witness IDs and private keys to configuration." );
  }
  ilog( "witness plugin:  plugin_startup() end" );
} FC_CAPTURE_AND_RETHROW() }

void witness_plugin::plugin_shutdown()
{
  try
  {
    if( !my->_is_p2p_enabled )
    {
      ilog("Witness plugin is not enabled, because P2P plugin is disabled...");
      return;
    }

    chain::util::disconnect_signal( my->_post_apply_block_conn );
    chain::util::disconnect_signal( my->_pre_apply_operation_conn );
    chain::util::disconnect_signal( my->_finish_push_block_conn );

    my->_timer.cancel();
  }
  catch(fc::exception& e)
  {
    edump( (e.to_detail_string()) );
  }
}

} } } // hive::plugins::witness

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::witness::witness_custom_op_index)
