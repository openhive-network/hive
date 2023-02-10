#include "node_based_conversion_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/variant.hpp>
#include <fc/variant_object.hpp>

#include <hive/chain/full_transaction.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/block.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <string>
#include <mutex>

#include "../base/conversion_plugin.hpp"

namespace hive { namespace converter { namespace plugins { namespace node_based_conversion {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

namespace detail {

  class node_based_conversion_plugin_impl final : public conversion_plugin_impl {
  public:

    node_based_conversion_plugin_impl( const std::string& input_url, const std::vector< std::string >& output_urls,
      const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id,
      size_t signers_size, size_t block_buffer_size, appbase::application& app );

    void close();

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;

    // Those two functions work with input_url which is always one
    fc::optional< hp::signed_block > receive_uncached( uint32_t num );
    fc::optional< hp::signed_block > receive( uint32_t block_num );

    const fc::variants& get_block_buffer()const;

    fc::url                input_url;
    std::vector< fc::url > output_urls;

    // Note: Since block numbers are stored as 32-bit integers in the current implementation of Hive, using a 64-bit integer
    // as the type for the last_block_range_start variable is a safe way to indicate whether a converter is in an "uninitialized" state.
    uint64_t             last_block_range_start = -1; // initial value to satisfy the `num < last_block_range_start` check to get new blocks on first call
    size_t               block_buffer_size;
    fc::variant_object   block_buffer_obj; // blocks buffer object containing lookup table

    appbase::application& theApp;
  };

  node_based_conversion_plugin_impl::node_based_conversion_plugin_impl( const std::string& input_url, const std::vector< std::string >& output_urls,
    const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size, size_t block_buffer_size, appbase::application& app )
    : conversion_plugin_impl( _private_key, chain_id, app, signers_size, false ), input_url( input_url ), block_buffer_size( block_buffer_size ), theApp( app )
  {
    FC_ASSERT( block_buffer_size && block_buffer_size <= 1000, "Blocks buffer size should be in the range 1-1000", ("block_buffer_size",block_buffer_size) );

    check_url(this->input_url);

    for( const auto& url : output_urls )
      check_url( this->output_urls.emplace_back(url) );
  }

  void node_based_conversion_plugin_impl::close()
  {
    if( !converter.has_hardfork( HIVE_HARDFORK_0_17__770 ) )
      wlog("Conversion interrupted before HF17. Pow authorities can still be added into the blockchain. Resuming the conversion without the saved converter state will result in corrupted block log");
  }

  void node_based_conversion_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    auto gpo = get_dynamic_global_properties( output_urls.at(0) );

    if( !start_block_num )
      start_block_num = gpo["head_block_number"].as< uint32_t >() + 1;

    ilog("Continuing conversion from the block with number ${block_num}", ("block_num", start_block_num));
    ilog("Validating the chain id...");

    fc::optional< hp::signed_block > block;

    // Iterate downwards to assign last block to the first of the specified node (according to the documentation)
    for( int64_t i = output_urls.size() - 1; i >= 0; --i )
    {
      // Null for every node
      block = fc::optional< hp::signed_block >{};

      const auto& url = output_urls.at(i);

      validate_chain_id( converter.get_chain_id(), url );
      dlog("Node \'${url}\': Chain id match", ("url", url));
    }

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

    const auto retry_handler = [&] {
      fc::usleep(fc::seconds(1));
      --start_block_num;
    };
    bool retry_flag = false;

    for( ; ( start_block_num <= stop_block_num || !stop_block_num ) && !theApp.is_interrupt_request(); ++start_block_num )
    {
      try
      {
        if( retry_flag )
        {
          retry_handler();
          retry_flag = false;
        }

        block = receive( start_block_num );

        if( !block.valid() )
        {
          wlog("Could not parse the block with number ${block_num} from the host. Propably the block has not been produced yet. Retrying in 1 second...", ("block_num", start_block_num));
          retry_flag = true;
          continue;
        }

        print_pre_conversion_data( *block );

        if( block->transactions.size() == 0 )
          continue; // Since we transmit only transactions, not entire blocks, we can skip block conversion if there are no transactions in the block

        auto transactions = converter.convert_signed_block( *block, lib_id,
          gpo["time"].as< time_point_sec >() + (HIVE_BLOCK_INTERVAL * gpo_interval) /* Deduce the now time */,
          true
        )->get_full_transactions();

        print_progress( start_block_num, stop_block_num );
        print_post_conversion_data( *block );

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
      } ICEBERG_GENERATE_CAPTURE_AND_LOG(retry_flag = true)
    }

    dlog("In order to resume your live conversion pass the \'-R ${block_num}\' option to the converter next time", ("block_num", start_block_num - 1));

    display_error_response_data();

    if( !theApp.is_interrupt_request() )
      theApp.generate_interrupt_request();
  }

  const fc::variants& node_based_conversion_plugin_impl::get_block_buffer()const
  {
    FC_ASSERT( block_buffer_obj.contains("blocks"), "There are no blocks present. It may be caused by either no receive requests"
      " performed yet or by the buffering stage completion" );

    return block_buffer_obj["blocks"].get_array();
  }

  fc::optional< hp::signed_block > node_based_conversion_plugin_impl::receive_uncached( uint32_t num )
  {
    try
    {
      fc::http::connection local_input_con;
      auto var_obj = post( local_input_con, input_url, "block_api.get_block", "{\"block_num\":" + std::to_string( num ) + "}" );

      if( !var_obj.contains("block") )
        return fc::optional< hp::signed_block >();

      return var_obj["block"].template as< hp::signed_block >();
    }
    catch( const error_response_from_node& error )
    {
      handle_error_response_from_node( error );

      return fc::optional< hp::signed_block >();
    } FC_CAPTURE_AND_RETHROW( (num) )
  }

  fc::optional< hp::signed_block > node_based_conversion_plugin_impl::receive( uint32_t num )
  {
    if( last_block_range_start != uint64_t(-1) && !block_buffer_obj.size() )
    { // Receive uncached if it is not a first receive and there are no properties in the `block_buffer_obj`
      return receive_uncached( num );
    } // Note: Outside of the main try/catch to prevent polluting of the fc exception stacktrace

    try
    {
      uint64_t result_offset = num - last_block_range_start;

      if( num < last_block_range_start // Initial check ( when last_block_range_start is uint64_t(-1) )
          || num >= last_block_range_start + block_buffer_size ) // number out of buffered blocks range
      {
        last_block_range_start = block_buffer_size == 1 ? num : num - ( num % block_buffer_size == 0 ? block_buffer_size : num % block_buffer_size ) + 1;
        result_offset = num - last_block_range_start; // Re-calculate the result offset after the `last_block_range_start` has been changed

        const std::string body = "{\"starting_block_num\":" + std::to_string( last_block_range_start ) + ",\"count\":" + std::to_string( block_buffer_size ) + "}";

        fc::http::connection local_input_con;
        auto var_obj = post( local_input_con, input_url, "block_api.get_block_range", body );

        FC_ASSERT( var_obj.contains("blocks"), "No blocks in JSON response", ("reply", var_obj) );
        // Ensures safety also removing redundant variant_object values copying
        FC_ASSERT( var_obj.size() == 1, "There should be only one entry (\'blocks\') in the \'block_api.get_block_range\' response" );

        block_buffer_obj = var_obj;
      }

      const auto& block_buf = get_block_buffer();
      // If there are no blocks, stop buffering and start uncached receive
      if( !block_buf.size() || result_offset >= block_buf.size() )
      {
        dlog("Started uncached receive on block ${block_num} to save your computers memory", ("block_num", num));
        // Clear block_buffer_obj to free now redundant memory and signal that buffering is done by removing `blocks` key
        block_buffer_obj = fc::variant_object{};
        return receive_uncached( num );
      }

      return get_block_buffer().at( result_offset ).template as< hp::signed_block >();
    }
    catch( const error_response_from_node& error )
    {
      handle_error_response_from_node( error );

      return fc::optional< hp::signed_block >();
    } FC_CAPTURE_AND_RETHROW( (num) )
  }

} // detail

  node_based_conversion_plugin::node_based_conversion_plugin() {}
  node_based_conversion_plugin::~node_based_conversion_plugin() {}

  void node_based_conversion_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg )
  {
    cfg.add_options()
      ( "block-buffer-size", bpo::value< size_t >()->default_value( 1000 ), "Block buffer size" )
      ( "use-now-time,T", bpo::bool_switch()->default_value( false ), "(deprecated) "
        "Set expiration time of the transactions to the current system time (works only with the node_based_conversion plugin)" );
  }

  void node_based_conversion_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    if( options.count("use-now-time") )
      wlog("\'--use-now-time\' option has been deprecated in favor of the relative transaction expiration times feature!");

    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " plugin" );
    FC_ASSERT( options.count("output"), "You have to specify the output source for the " HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " plugin" );

    auto input_v = options["input"].as< std::vector< std::string > >();
    auto output_v = options["output"].as< std::vector< std::string > >();

    FC_ASSERT( input_v.size() == 1, HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " accepts only one input node" );
    FC_ASSERT( output_v.size(), HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " requires at least one output node" );

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

    my = std::make_unique< detail::node_based_conversion_plugin_impl >(
          input_v.at(0), output_v,
          *private_key, _hive_chain_id, options.at( "jobs" ).as< size_t >(),
          options["block-buffer-size"].as< size_t >(), get_app()
        );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );
  }

  void node_based_conversion_plugin::plugin_startup() {
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
  void node_based_conversion_plugin::plugin_shutdown()
  {
    my->print_wifs();
    my->close();
  }

} } } } // hive::converter::plugins::block_log_conversion
