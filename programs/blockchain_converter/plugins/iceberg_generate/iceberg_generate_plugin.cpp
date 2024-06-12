#include "iceberg_generate_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant.hpp>
#include <fc/network/url.hpp>
#include <fc/thread/thread.hpp>

#include <hive/chain/block_log_wrapper.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/block.hpp>
#include <hive/protocol/hardfork_block.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include <boost/program_options.hpp>
#include <boost/container/flat_set.hpp>

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <map>

#include "../base/conversion_plugin.hpp"

#include "ops_permlink_tracker.hpp"
#include "ops_required_asset_transfer_visitor.hpp"
#include "ops_strip_content_visitor.hpp"
#include "ops_impacted_accounts_visitor.hpp"

namespace hive {namespace converter { namespace plugins { namespace iceberg_generate {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

namespace detail {

  using hive::chain::block_log_wrapper;

  class iceberg_generate_plugin_impl final : public conversion_plugin_impl {
  public:
    std::vector< fc::url > output_urls;
    bool enable_op_content_strip;
    appbase::application& theApp;
    hive::chain::blockchain_worker_thread_pool thread_pool;
    fc::optional<hp::transaction_id_type> last_init_tx_id;
    std::shared_ptr< block_log_wrapper > log_reader;

    iceberg_generate_plugin_impl( const std::vector< std::string >& output_urls, const hp::private_key_type& _private_key,
        const hp::chain_id_type& chain_id, appbase::application& app, bool enable_op_content_strip = false, size_t signers_size = 1 );

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;
    void open( const fc::path& input );
    void close();

    void prepare_blockchain( uint32_t start_block_num, uint32_t stop_block_num );

    void wait_blockchain_ready();

    std::shared_ptr<hc::full_transaction_type> generate_bootstrap_witness_tx_for(
      const hp::account_name_type& wit,
      uint32_t requested_max_block_size,
      uint32_t account_creation_fee,
      const hp::block_id_type& previous_block_id,
      fc::time_point_sec head_block_time
    );
    hp::transfer_to_vesting_operation generate_vesting_for_account( const hp::account_name_type& acc )const;
    hp::transfer_operation generate_hbd_transfer_for_account( const hp::account_name_type& acc )const;
    hp::transfer_operation generate_hive_transfer_for_account( const hp::account_name_type& acc )const;
    hp::account_create_operation on_new_account_collected( const hp::account_name_type& acc, const hp::asset& account_creation_fee )const;
    hp::comment_operation on_comment_collected( const hp::account_name_type& acc, const std::string& link )const;
  };


  iceberg_generate_plugin_impl::iceberg_generate_plugin_impl( const std::vector< std::string >& output_urls, const hp::private_key_type& _private_key,
      const hp::chain_id_type& chain_id, appbase::application& app, bool enable_op_content_strip, size_t signers_size )
    :  conversion_plugin_impl( _private_key, chain_id, app, signers_size, true ), enable_op_content_strip( enable_op_content_strip ), theApp( app ),
       thread_pool( hive::chain::blockchain_worker_thread_pool( app ) )
  {
    for( const auto& url : output_urls )
      check_url( this->output_urls.emplace_back(url) );

    converter.increase_transaction_expiration_time = true;
  }

  void iceberg_generate_plugin_impl::open( const fc::path& input )
  {
    try
    {
      log_reader = hive::chain::block_log_wrapper::create_opened_wrapper( input, theApp, thread_pool, true /*read_only*/ );
    } FC_CAPTURE_AND_RETHROW( (input) );
  }

  std::shared_ptr<hc::full_transaction_type> iceberg_generate_plugin_impl::generate_bootstrap_witness_tx_for(
    const hp::account_name_type& wit,
    uint32_t requested_max_block_size,
    uint32_t account_creation_fee,
    const hp::block_id_type& previous_block_id,
    fc::time_point_sec head_block_time
  )
  {
    hp::signed_transaction tx;

    hp::witness_update_operation op;
    op.owner = wit;
    op.url = "#";
    op.block_signing_key = converter.get_witness_key().get_public_key();

    hp::legacy_chain_properties props{};
    props.account_creation_fee = hp::legacy_hive_asset::from_amount( account_creation_fee );
    props.maximum_block_size = requested_max_block_size;

    op.props = props;
    op.fee = hp::asset( 1, HIVE_SYMBOL );

    tx.operations.push_back(op);
    tx.signatures.emplace_back();

    return converter.convert_signed_transaction(
      tx,
      previous_block_id,
      [&](hp::transaction& trx) {
        trx.expiration = head_block_time + HIVE_MAX_TIME_UNTIL_EXPIRATION - HIVE_BC_SAFETY_TIME_GAP;
      },
      0,
      account_creation_fee,
      true
    );
  }

  hp::transfer_to_vesting_operation iceberg_generate_plugin_impl::generate_vesting_for_account( const hp::account_name_type& acc )const
  {
    static const uint64_t hive_precision = std::pow(10, HIVE_PRECISION_HIVE);

    hp::transfer_to_vesting_operation op;
    op.from = HIVE_INIT_MINER_NAME;
    op.to = acc;
    op.amount = hp::asset{ 100 * hive_precision, HIVE_SYMBOL };

    return op;
  }

  hp::transfer_operation iceberg_generate_plugin_impl::generate_hbd_transfer_for_account( const hp::account_name_type& acc )const
  {
    static const uint64_t hbd_precision = std::pow(10, HIVE_PRECISION_HBD);

    hp::transfer_operation op;
    op.from = HIVE_INIT_MINER_NAME;
    op.to = acc;
    op.amount = hp::asset{ 100 * hbd_precision, HBD_SYMBOL };

    return op;
  }

  hp::transfer_operation iceberg_generate_plugin_impl::generate_hive_transfer_for_account( const hp::account_name_type& acc )const
  {
    static const uint64_t hive_precision = std::pow(10, HIVE_PRECISION_HIVE);

    hp::transfer_operation op;
    op.from = HIVE_INIT_MINER_NAME;
    op.to = acc;
    op.amount = hp::asset{ 100 * hive_precision, HIVE_SYMBOL };

    return op;
  }

  hp::account_create_operation iceberg_generate_plugin_impl::on_new_account_collected( const hp::account_name_type& acc, const hp::asset& account_creation_fee )const
  {
    hp::account_create_operation op;
    op.fee = account_creation_fee;
    op.creator = HIVE_INIT_MINER_NAME;
    op.new_account_name = acc;
    op.memo_key = converter.get_second_authority_key( hp::authority::classification::owner ).get_public_key();

    return op;
  }

  hp::comment_operation iceberg_generate_plugin_impl::on_comment_collected( const hp::account_name_type& acc, const std::string& link )const
  {
    hp::comment_operation op;
    op.body = "#";
    op.parent_author = HIVE_ROOT_POST_PARENT;
    op.parent_permlink = "x";
    op.author = acc;
    op.permlink = link;

    return op;
  }

  void iceberg_generate_plugin_impl::wait_blockchain_ready()
  {
    std::vector<hp::account_name_type> witnesses;

    // Collect witness, wait for the blockchain to be ready
    while(true)
    {
      witnesses = get_current_shuffled_witnesses( output_urls.at(0) );

      ilog("${sw} shuffled witnesses detected", ("sw", witnesses.size()));
      if(witnesses.size() > 1)
        break;

      dlog("Waiting one block interval for the witnesses to appear");
      fc::usleep(fc::seconds(HIVE_BLOCK_INTERVAL));
    }

    uint32_t requested_maximum_block_size = HIVE_MAX_BLOCK_SIZE;

    auto gpo = get_dynamic_global_properties( output_urls.at(0) );

    hp::block_id_type head_block_id = gpo["head_block_id"].template as< hp::block_id_type >();
    fc::time_point_sec head_block_time = gpo["time"].template as<fc::time_point_sec>();

    // Update maximum_block_size and any other required blockchain properties
    for(const auto& wit : witnesses)
    {
      dlog("Updating witness properties for the witness: ${wit}", (wit));
      transmit(*generate_bootstrap_witness_tx_for(
        wit,
        requested_maximum_block_size,
        HIVE_MIN_ACCOUNT_CREATION_FEE,
        head_block_id,
        head_block_time
      ), output_urls.at(0));
    }

    // Wait for the next witness schedule for the properties to be applied
    while(true)
    {
      gpo = get_dynamic_global_properties( output_urls.at(0) );
      uint32_t max_block_size = gpo["maximum_block_size"].as< uint32_t >();

      if(max_block_size == requested_maximum_block_size)
        break;

      dlog("Waiting one block interval for properties to be applied");
      fc::usleep(fc::seconds(HIVE_BLOCK_INTERVAL));
    }

    ilog("Blockchain properties set.");
  }

  void iceberg_generate_plugin_impl::prepare_blockchain( uint32_t start_block_num, uint32_t stop_block_num )
  {
    auto gpo = get_dynamic_global_properties( output_urls.at(0) );
    // Last irreversible block number and id for tapos generation
    uint32_t lib_num = gpo["last_irreversible_block_num"].as< uint32_t >();
    hp::block_id_type lib_id = get_previous_from_block( lib_num, output_urls.at(0) );

    const auto update_lib_id = [&]() {
      try
      {
        // Update dynamic global properties object and check if there is a new irreversible block
        // If so, then update lib id
        gpo = get_dynamic_global_properties( output_urls.at(0) );
        uint32_t new_lib_num = gpo["last_irreversible_block_num"].as< uint32_t >();
        if( lib_num != new_lib_num )
        {
          lib_num = new_lib_num;
          lib_id  = get_previous_from_block( lib_num, output_urls.at(0) );
        }
      }
      catch( const error_response_from_node& error )
      {
        handle_error_response_from_node( error );
      }
    };

    hp::asset account_creation_fee = get_account_creation_fee( output_urls.at(0) );
    ops_strip_content_visitor ops_strip_content{};

    uint32_t gpo_interval = 0;
    const auto init_start_block_num = start_block_num;
    uint32_t last_witness_schedule_block_check = lib_num;

    std::map< uint32_t, hp::share_type > init_assets;

    boost::container::flat_set<hp::account_name_type> all_accounts = {
      HIVE_MINER_ACCOUNT, HIVE_NULL_ACCOUNT, HIVE_TEMP_ACCOUNT, HIVE_INIT_MINER_NAME, "steem", OBSOLETE_TREASURY_ACCOUNT, NEW_HIVE_TREASURY_ACCOUNT
    };
    boost::container::flat_set<author_and_permlink_hash_t> all_permlinks;

    // Pre-init: Detect required supply and dependent comments and accounts
    ilog("Checking required initial supply");
    for( ; start_block_num <= stop_block_num && !theApp.is_interrupt_request(); ++start_block_num )
    {
      try {
        if( lib_num - last_witness_schedule_block_check >= HIVE_MAX_WITNESSES )
        {
          account_creation_fee = get_account_creation_fee( output_urls.at(0) );
          last_witness_schedule_block_check = lib_num;
        }

        std::shared_ptr<hive::chain::full_block_type> _full_block = log_reader->read_block_by_num( start_block_num );
        FC_ASSERT( _full_block, "unable to read block", ("block_num", start_block_num) );

        hp::signed_block block = _full_block->get_block(); // Copy required due to the const reference returned by the get_block function
        converter.touch(block); // converter.convert_signed_transaction() does not apply hardforks, so we have to do it manually

        boost::container::flat_set<hp::account_name_type> new_accounts;
        hp::signed_transaction dependents_tx;
        hp::signed_transaction comments_tx;

        if( block.transactions.size() == 0 )
          continue; // Since we transmit only transactions, not entire blocks, we can skip block conversion if there are no transactions in the block

        for( size_t i = 0; i < block.transactions.size(); ++i )
        {
          std::vector<ops_permlink_tracker_result_t> permlinks;

          for( size_t j = 0; j < block.transactions.at(i).operations.size(); ++j )
          {
            auto& op = block.transactions.at(i).operations.at(j);

            for(const auto& balance : hive::app::operation_get_impacted_balances(op, blockchain_converter::has_hardfork( HIVE_HARDFORK_0_1, start_block_num )))
            {
              uint32_t nai = balance.second.symbol.to_nai();

              if( init_assets.find(nai) == init_assets.end() )
                init_assets[nai] = balance.second.amount.value;
              else
                init_assets[nai] += balance.second.amount.value;
            }

            op.visit( ops_impacted_accounts_visitor{ new_accounts, all_accounts, converter } );

            // Collecting permlinks
            const auto created_permlink_data = op.visit(created_permlinks_visitor{});
            all_permlinks.insert( compute_author_and_permlink_hash( created_permlink_data ) );

            permlinks.emplace_back( op.visit(dependent_permlinks_visitor{}) );

            // Stripping operations content
            if( enable_op_content_strip )
              op = op.visit( ops_strip_content );
          }

          dependents_tx.operations.reserve(new_accounts.size() * 4);
          comments_tx.operations.reserve(permlinks.size());

          for( const auto& acc : new_accounts )
            if( all_accounts.insert(acc).second )
            {
              dependents_tx.operations.emplace_back(on_new_account_collected(acc, account_creation_fee));
              dependents_tx.operations.emplace_back(generate_vesting_for_account(acc));
              dependents_tx.operations.emplace_back(generate_hbd_transfer_for_account(acc));
              dependents_tx.operations.emplace_back(generate_hive_transfer_for_account(acc));
            }

          for( const auto& dependent_permlink_data : permlinks )
            if( dependent_permlink_data.first.size() && all_permlinks.insert( compute_author_and_permlink_hash( dependent_permlink_data ) ).second )
              comments_tx.operations.emplace_back(on_comment_collected(dependent_permlink_data.first, dependent_permlink_data.second));
        }

        const auto head_block_time = gpo["time"].as< fc::time_point_sec >() + (HIVE_BLOCK_INTERVAL * gpo_interval);
        uint32_t block_offset = uint32_t( std::abs( (head_block_time - block.timestamp).to_seconds() ) );

        // Note: We do not use pow transactions in the initial iceberg converter stage, so handling helper_pow_transaction is redundant
        if( dependents_tx.operations.size() > 0 )
        {
          const auto tx_converted = converter.convert_signed_transaction( dependents_tx, lib_id,
            [&](hp::transaction& trx) {
              trx.expiration = head_block_time + HIVE_MAX_TIME_UNTIL_EXPIRATION - HIVE_BC_SAFETY_TIME_GAP;
            },
            block_offset,
            account_creation_fee.amount.value,
            true
          );

          try {
            transmit( *tx_converted, output_urls.at( 0 ) );
          } ICEBERG_GENERATE_CAPTURE_AND_LOG()
          last_init_tx_id = tx_converted->get_transaction_id();
        }

        if( comments_tx.operations.size() > 0 )
        {
          const auto tx_converted = converter.convert_signed_transaction( comments_tx, lib_id,
            [&](hp::transaction& trx) {
              trx.expiration = head_block_time + HIVE_MAX_TIME_UNTIL_EXPIRATION - HIVE_BC_SAFETY_TIME_GAP;
            },
            block_offset,
            account_creation_fee.amount.value,
            true
          );

          try {
            transmit( *tx_converted, output_urls.at( 0 ) );
          } ICEBERG_GENERATE_CAPTURE_AND_LOG()
          last_init_tx_id = tx_converted->get_transaction_id();
        }

        print_progress( start_block_num - init_start_block_num, stop_block_num - init_start_block_num );

        gpo_interval = start_block_num % HIVE_BC_TIME_BUFFER;

        if( gpo_interval == 0 )
        {
          update_lib_id();
          converter.on_tapos_change();
        }
      } ICEBERG_GENERATE_CAPTURE_AND_LOG()
    }

    int64_t virtual_supply = gpo["virtual_supply"].as< hp::asset >().amount.value;
    int64_t current_hbd_supply = gpo["current_hbd_supply"].as< hp::asset >().amount.value;
    int64_t init_hbd_supply = gpo["init_hbd_supply"].as< hp::asset >().amount.value;

    // Use current hbd supply if there is any, otherwise use init hbd supply (can be either 0 or the value from the alternate chain properties file)
    int64_t real_hbd_supply = current_hbd_supply == 0 ? init_hbd_supply : current_hbd_supply;

    ilog("Init supply: [${is} HIVE required, available: ${vs} HIVE], HBD Init supply: [${his} HBD required, available: ${hvs} HBD]",
      ("is", init_assets[HIVE_NAI_HIVE])("vs", virtual_supply)
      ("his", init_assets[HIVE_NAI_HBD])("hvs", real_hbd_supply)
    );

    // XXX: Should we also handle HIVE_NAI_VESTS?
    FC_ASSERT( virtual_supply >= init_assets[HIVE_NAI_HIVE], "Insufficient initial supply in the output node blockchain" );
    FC_ASSERT( real_hbd_supply >= init_assets[HIVE_NAI_HBD], "Insufficient HBD initial supply in the output node blockchain" );

    ilog("Blockchain ready for the conversion.");
  }

  void iceberg_generate_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    FC_ASSERT( log_reader, "Input block log should be opened before the conversion" );
    FC_ASSERT( log_reader->head_block(), "Your input block log is empty" );

    FC_ASSERT( start_block_num,
      HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin currently does not currently support conversion continue" );

    if( !stop_block_num || stop_block_num > log_reader->head_block()->get_block_num() )
      stop_block_num = log_reader->head_block()->get_block_num();

    auto gpo = get_dynamic_global_properties( output_urls.at(0) );
    // Last irreversible block number and id for tapos generation
    uint32_t lib_num = gpo["last_irreversible_block_num"].as< uint32_t >();
    hp::block_id_type lib_id = get_previous_from_block( lib_num, output_urls.at(0) );

    const auto update_lib_id = [&]() {
      try
      {
        // Update dynamic global properties object and check if there is a new irreversible block
        // If so, then update lib id
        gpo = get_dynamic_global_properties( output_urls.at(0) );
        uint32_t new_lib_num = gpo["last_irreversible_block_num"].as< uint32_t >();
        if( lib_num != new_lib_num )
        {
          lib_num = new_lib_num;
          lib_id  = get_previous_from_block( lib_num, output_urls.at(0) );
        }
      }
      catch( const error_response_from_node& error )
      {
        handle_error_response_from_node( error );
      }
    };

    uint32_t gpo_interval = 0;

    const auto init_start_block_num = start_block_num;

    uint32_t last_witness_schedule_block_check = lib_num;
    hp::asset account_creation_fee = get_account_creation_fee( output_urls.at(0) );

    ops_strip_content_visitor ops_strip_content{};

    wait_blockchain_ready();

    prepare_blockchain( start_block_num, stop_block_num );

    update_lib_id();
    ilog("Initial supply requirements met");

    if (last_init_tx_id.valid())
    {
      ilog("Waiting for the '${last_init_tx_id}' tx to be applied...", (last_init_tx_id));
      while( !transaction_applied(output_urls.at(0), *last_init_tx_id) && !theApp.is_interrupt_request() )
      {
        ilog("Transaction '${last_init_tx_id}' has not been applied. Retrying in one block interval...", (last_init_tx_id));
        fc::usleep(fc::seconds(HIVE_BLOCK_INTERVAL));
      }

      ilog("Transaction '${last_init_tx_id}' applied in block.", (last_init_tx_id));
    }

    // The actual conversion:
    for( ; start_block_num <= stop_block_num && !theApp.is_interrupt_request(); ++start_block_num )
    {
      try {
        if( lib_num - last_witness_schedule_block_check >= HIVE_MAX_WITNESSES )
        {
          account_creation_fee = get_account_creation_fee( output_urls.at(0) );
          last_witness_schedule_block_check = lib_num;
        }

        std::shared_ptr<hive::chain::full_block_type> _full_block = log_reader->read_block_by_num( start_block_num );
        FC_ASSERT( _full_block, "unable to read block", ("block_num", start_block_num) );

        hp::signed_block block = _full_block->get_block(); // Copy required due to the const reference returned by the get_block function
        print_pre_conversion_data( block );

        if( block.transactions.size() == 0 )
          continue; // Since we transmit only transactions, not entire blocks, we can skip block conversion if there are no transactions in the block

        if( enable_op_content_strip )
          for( auto& tx : block.transactions )
            for( auto& op : tx.operations )
                op = op.visit( ops_strip_content );

        auto block_converted = converter.convert_signed_block( block, lib_id,
          gpo["time"].as< fc::time_point_sec >() + (HIVE_BLOCK_INTERVAL * gpo_interval) /* Deduce the now time */,
          true,
          account_creation_fee.amount.value
        );

        print_progress( start_block_num - init_start_block_num, stop_block_num - init_start_block_num );
        print_post_conversion_data( block_converted->get_block() );

        const auto& transactions = block_converted->get_full_transactions();
        FC_ASSERT(transactions.size() == block_converted->get_block().transactions.size());

        for( size_t i = 0; i < transactions.size(); ++i )
          if( theApp.is_interrupt_request() ) // If there were multiple trxs in block user would have to wait for them to transmit before exiting without this check
            break;
          else
            try {
              transmit( *transactions.at(i), output_urls.at( i % output_urls.size() ) );
            } ICEBERG_GENERATE_CAPTURE_AND_LOG()

        gpo_interval = start_block_num % HIVE_BC_TIME_BUFFER;

        if( gpo_interval == 0 )
        {
          update_lib_id();
          converter.on_tapos_change();
        }
      } ICEBERG_GENERATE_CAPTURE_AND_LOG()
    }

    display_error_response_data();

    if( !theApp.is_interrupt_request() )
      theApp.generate_interrupt_request();
  }

  void iceberg_generate_plugin_impl::close()
  {
    if( log_reader )
      log_reader->close_storage();

    if( !converter.has_hardfork( HIVE_HARDFORK_0_17__770 ) )
      wlog("Conversion interrupted before HF17. Pow authorities can still be added into the blockchain. Resuming the conversion without the saved converter state will result in corrupted block log");
  }

} // detail

  iceberg_generate_plugin::iceberg_generate_plugin() {}
  iceberg_generate_plugin::~iceberg_generate_plugin() {}

  void iceberg_generate_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg )
  {
    cfg.add_options()
      // TODO: Requires documentation:
      ( "strip-operations-content,X", bpo::bool_switch()->default_value( false ), "Strips redundant content of some operations like posts and json operations content" );
  }

  void iceberg_generate_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin" );
    FC_ASSERT( options.count("output"), "You have to specify the output source for the " HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin" );

    auto input_v = options["input"].as< std::vector< std::string > >();
    auto output_v = options["output"].as< std::vector< std::string > >();

    FC_ASSERT( input_v.size() == 1, HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " accepts only one input block log" );
    FC_ASSERT( output_v.size(), HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " requires at least one output node" );

    idump((input_v)(output_v));

    std::string out_file;
    if( options.count("output") && options["output"].as< std::vector< std::string > >().size() )
      out_file = options["output"].as< std::vector< std::string > >().at(0);
    else
      out_file = input_v.at(0) + "_out";

    fc::path block_log_in( input_v.at(0) );

    hp::chain_id_type _hive_chain_id;

    const auto& chain_id_str = options["chain-id"].as< std::string >();

    try
    {
      _hive_chain_id = hp::chain_id_type( chain_id_str );
    }
    catch( fc::exception& )
    {
      FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
    }

    const auto private_key = fc::ecc::private_key::wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse the private key" );

    my = std::make_unique< detail::iceberg_generate_plugin_impl >( output_v, *private_key, _hive_chain_id, get_app(), options.count("strip-operations-content"), options.at( "jobs" ).as< size_t >() );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );

    my->open( block_log_in );
  }

  void iceberg_generate_plugin::plugin_startup() {
    try
    {
#ifdef HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED
      my->convert(
        get_app().get_args().at("resume-block").as< uint32_t >(),
        get_app().get_args().at( "stop-block" ).as< uint32_t >()
      );
#else
      FC_THROW("Blockchain converter has been built without the iceberg_generate plugin enabled. Please enable it in order to run it under hived.");
#endif
    }
    catch( const fc::exception& e )
    {
      elog( e.to_detail_string() );
      get_app().generate_interrupt_request();
    }
  }
  void iceberg_generate_plugin::plugin_shutdown()
  {
    my->close();
    my->print_wifs();
  }

} } } } // hive::converter::plugins::block_log_conversion
