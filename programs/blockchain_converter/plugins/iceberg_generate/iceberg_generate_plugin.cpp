#include "iceberg_generate_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant.hpp>
#include <fc/network/url.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/block.hpp>
#include <hive/protocol/hardfork_block.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <boost/program_options.hpp>
#include <boost/container/flat_set.hpp>

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "../base/conversion_plugin.hpp"

#include "ops_permlink_tracker.hpp"
#include "ops_required_asset_transfer_visitor.hpp"
#include "ops_strip_content_visitor.hpp"

namespace hive {namespace converter { namespace plugins { namespace iceberg_generate {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

  using hive::utilities::wif_to_key;

namespace detail {

  using hive::chain::block_log;

  class iceberg_generate_plugin_impl final : public conversion_plugin_impl {
  public:
    block_log log_in;
    std::vector< fc::url > output_urls;
    bool enable_op_content_strip;

    iceberg_generate_plugin_impl( const std::vector< std::string >& output_urls, const hp::private_key_type& _private_key,
        const hp::chain_id_type& chain_id, bool enable_op_content_strip = false, size_t signers_size = 1 );

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;
    void open( const fc::path& input );
    void close();

    void on_new_account_collected( const hp::account_name_type& acc );
    void on_comment_collected( const hp::account_name_type& acc, const std::string& link );
    void transfer_required_asset( const hp::account_name_type& acc, const hp::asset& bal );
  };


  iceberg_generate_plugin_impl::iceberg_generate_plugin_impl( const std::vector< std::string >& output_urls, const hp::private_key_type& _private_key,
      const hp::chain_id_type& chain_id, bool enable_op_content_strip, size_t signers_size )
    :  conversion_plugin_impl( _private_key, chain_id, signers_size, true )
  {
    idump((output_urls));

    static const auto check_url = []( const auto& url ) {
      FC_ASSERT( url.proto() == "http", "Currently only http protocol is supported", ("out_proto", url.proto()) );
      FC_ASSERT( url.host().valid(), "You have to specify the host in url", ("url",url) );
      FC_ASSERT( url.port().valid(), "You have to specify the port in url", ("url",url) );
    };

    for( const auto& url : output_urls )
      check_url( this->output_urls.emplace_back(url) );
  }

  void iceberg_generate_plugin_impl::open( const fc::path& input )
  {
    try
    {
      log_in.open( input );
    } FC_CAPTURE_AND_RETHROW( (input) );
  }

  void iceberg_generate_plugin_impl::on_new_account_collected( const hp::account_name_type& acc )
  {
    ilog("Collected new account: ${acc}", ("acc", acc));
  }

  void iceberg_generate_plugin_impl::on_comment_collected( const hp::account_name_type& acc, const std::string& link )
  {
    ilog("Collected new permlink: /@${acc}/${link}", ("acc", acc)("link", link));
  }

  void iceberg_generate_plugin_impl::transfer_required_asset( const hp::account_name_type& acc, const hp::asset& bal )
  {
    ilog("Collected required asset for account ${acc}: ${bal}", ("acc", acc)("bal", bal));
  }

  void iceberg_generate_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    FC_ASSERT( log_in.is_open(), "Input block log should be opened before the conversion" );
    FC_ASSERT( log_in.head(), "Your input block log is empty" );

    fc::time_point_sec head_block_time = HIVE_GENESIS_TIME;

    FC_ASSERT( start_block_num,
      HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin currently does not currently support conversion continue" );

    hp::block_id_type last_block_id;

    if( !stop_block_num || stop_block_num > log_in.head()->get_block_num() )
      stop_block_num = log_in.head()->get_block_num();

    boost::container::flat_set<hp::account_name_type> all_accounts;
    boost::container::flat_set<author_and_permlink_hash_t> all_permlinks;

    // Pre-init: Detect required iceberg operations
    for( ; start_block_num <= stop_block_num && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      std::shared_ptr<hive::chain::full_block_type> _full_block = log_in.read_block_by_num( start_block_num );
      FC_ASSERT( _full_block, "unable to read block", ("block_num", start_block_num) );

      hp::signed_block block = _full_block->get_block(); // Copy required due to the const reference returned by the get_block function
      print_pre_conversion_data( block );

      block.extensions.clear();

      auto fb = converter.convert_signed_block( block, last_block_id, head_block_time, false );
      last_block_id = fb->get_block_id();
      converter.on_tapos_change();

      for( auto& tx : block.transactions )
        for( auto& op : tx.operations )
        {
          // Stripping operations content
          if( enable_op_content_strip )
            op = op.visit( ops_strip_content_visitor{} );

          // Collecting impacted accounts - should be always before collecting permlinks
          boost::container::flat_set<hp::account_name_type> new_accounts;

          hive::app::operation_get_impacted_accounts( op, new_accounts );
          for( const auto& acc : new_accounts )
            if( all_accounts.insert(acc).second )
              on_new_account_collected(acc);

          // Collecting permlinks
          const auto created_permlink_data = op.visit(created_permlinks_visitor{});
          all_permlinks.insert( compute_author_and_permlink_hash( created_permlink_data ) );

          const auto& dependent_permlink_data = op.visit(dependent_permlinks_visitor{});
          if( dependent_permlink_data.first.size() && all_permlinks.insert( compute_author_and_permlink_hash( dependent_permlink_data ) ).second )
            on_comment_collected(dependent_permlink_data.first, dependent_permlink_data.second);

          const auto required_assets_accounts = op.visit(ops_required_asset_transfer_visitor{ converter.get_cached_hardfork() });
          for( const auto& [ account, assets ] : required_assets_accounts )
            for( const auto& asset : assets )
              transfer_required_asset(account, asset);
        }

      // Broadcast transactions here. Ignore creation of OBSOLETE_TREASURY_ACCOUNT and NEW_HIVE_TREASURY_ACCOUNT

      print_progress( start_block_num, stop_block_num );
      print_post_conversion_data( block );

      head_block_time = block.timestamp;
    }

    if( !appbase::app().is_interrupt_request() )
      appbase::app().generate_interrupt_request();
  }

  void iceberg_generate_plugin_impl::close()
  {
    if( log_in.is_open() )
      log_in.close();

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

    const auto private_key = wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse the private key" );

    my = std::make_unique< detail::iceberg_generate_plugin_impl >( output_v, *private_key, _hive_chain_id, options.count("strip-operations-content"), options.at( "jobs" ).as< size_t >() );

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
        appbase::app().get_args().at("resume-block").as< uint32_t >(),
        appbase::app().get_args().at( "stop-block" ).as< uint32_t >()
      );
#else
      FC_THROW("Blockchain converter has been built without the iceberg_generate plugin enabled. Please enable it in order to run it under hived.");
#endif
    }
    catch( const fc::exception& e )
    {
      elog( e.to_detail_string() );
      appbase::app().generate_interrupt_request();
    }
  }
  void iceberg_generate_plugin::plugin_shutdown()
  {
    my->close();
    my->print_wifs();
  }

} } } } // hive::converter::plugins::block_log_conversion
