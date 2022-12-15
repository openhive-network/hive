#include "iceberg_generate_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant.hpp>

#include <hive/chain/block_log.hpp>
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

#include "conversion_plugin.hpp"

#include "converter.hpp"
#include "hive/protocol/hive_operations.hpp"

#include "ops_strip_content_visitor.hpp"

namespace hive {namespace converter { namespace plugins { namespace iceberg_generate {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

namespace detail {

  using hive::chain::block_log;

  class iceberg_generate_plugin_impl final : public conversion_plugin_impl {
  public:
    block_log log_in, log_out;
    bool enable_op_content_strip;

    appbase::application& theApp;
    hive::chain::blockchain_worker_thread_pool thread_pool;

    iceberg_generate_plugin_impl( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, appbase::application& app, bool enable_op_content_strip = false, size_t signers_size = 1 )
      : conversion_plugin_impl( _private_key, chain_id, signers_size, app, true ), log_in( app ), log_out( app ),
        enable_op_content_strip(enable_op_content_strip), theApp( app ), thread_pool( hive::chain::blockchain_worker_thread_pool( app ) ) {}

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;
    void open( const fc::path& input, const fc::path& output );
    void close();

    void on_new_accounts_collected( const boost::container::flat_set<hp::account_name_type>& acc );
  };

  void iceberg_generate_plugin_impl::open( const fc::path& input, const fc::path& output )
  {
    try
    {
      log_in.open( input, thread_pool );
    } FC_CAPTURE_AND_RETHROW( (input) )

    try
    {
      log_out.open( output, thread_pool );
    } FC_CAPTURE_AND_RETHROW( (output) )
  }

  void iceberg_generate_plugin_impl::on_new_accounts_collected( const boost::container::flat_set<hp::account_name_type>& accs ) {
    for( const auto& acc : accs )
      ilog("Collected new account: ${acc}", ("acc", acc));
  }

  void iceberg_generate_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    FC_ASSERT( log_in.is_open(), "Input block log should be opened before the conversion" );
    FC_ASSERT( log_out.is_open(), "Output block log should be opened before the conversion" );
    FC_ASSERT( log_in.head(), "Your input block log is empty" );

    fc::time_point_sec head_block_time = HIVE_GENESIS_TIME;

    FC_ASSERT( !log_out.head() && start_block_num,
      HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin currently does not currently support conversion continue" );

    hp::block_id_type last_block_id;

    if( !stop_block_num || stop_block_num > log_in.head()->get_block_num() )
      stop_block_num = log_in.head()->get_block_num();

    boost::container::flat_set<hp::account_name_type> all_accounts;

    // Pre-init: Detect required iceberg operations
    for( ; start_block_num <= stop_block_num && !theApp.is_interrupt_request(); ++start_block_num )
    {
      std::shared_ptr<hive::chain::full_block_type> _full_block = log_in.read_block_by_num( start_block_num );
      FC_ASSERT( _full_block, "unable to read block", ("block_num", start_block_num) );

      hp::signed_block block = _full_block->get_block(); // Copy required due to the const reference returned by the get_block function

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
        dlog("Rewritten block: ${block_num}. Data before conversion: ${block}", ("block_num", start_block_num)("block", block));

      block.extensions.clear();

      for( auto& tx : block.transactions )
        for( auto& op : tx.operations )
        {
          if( enable_op_content_strip )
            op = op.visit( ops_strip_content_visitor{} );

          boost::container::flat_set<hp::account_name_type> new_accounts;

          hive::app::operation_get_impacted_accounts( op, new_accounts );
          on_new_accounts_collected(new_accounts);
          all_accounts.merge(std::move(new_accounts));
        }

      auto fb = converter.convert_signed_block( block, last_block_id, head_block_time, false );
      last_block_id = fb->get_block_id();
      converter.on_tapos_change();

      if( start_block_num % 1000 == 0 ) // Progress
        ilog("[ ${progress}% ]: ${processed}/${stop_point} blocks rewritten",
          ("progress", int( float(start_block_num) / stop_block_num * 100 ))("processed", start_block_num)("stop_point", stop_block_num));

      log_out.append( fb, false );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
        dlog("After conversion: ${block}", ("block", block));

      head_block_time = block.timestamp;
    }

    if( !theApp.is_interrupt_request() )
      theApp.generate_interrupt_request();
  }

  void iceberg_generate_plugin_impl::close()
  {
    if( log_in.is_open() )
      log_in.close();

    if( log_out.is_open() )
      log_out.close();

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

    auto input_v = options["input"].as< std::vector< std::string > >();

    FC_ASSERT( input_v.size() == 1, HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " accepts only one input block log" );

    std::string out_file;
    if( options.count("output") && options["output"].as< std::vector< std::string > >().size() )
      out_file = options["output"].as< std::vector< std::string > >().at(0);
    else
      out_file = input_v.at(0) + "_out";

    fc::path block_log_in( input_v.at(0) );
    fc::path block_log_out( out_file );

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

    my = std::make_unique< detail::iceberg_generate_plugin_impl >( *private_key, _hive_chain_id, get_app(), options.count("strip-operations-content"), options.at( "jobs" ).as< size_t >() );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );

    my->open( block_log_in, block_log_out );
  }

  void iceberg_generate_plugin::plugin_startup() {
    try
    {
      my->convert(
        get_app().get_args().at("resume-block").as< uint32_t >(),
        get_app().get_args().at( "stop-block" ).as< uint32_t >()
      );
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
