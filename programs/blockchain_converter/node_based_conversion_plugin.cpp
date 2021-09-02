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

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/block.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <hive/plugins/condenser_api/condenser_api_legacy_objects.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <string>
#include <iostream>

#include "conversion_plugin.hpp"

#include "converter.hpp"

namespace hive { namespace converter { namespace plugins { namespace node_based_conversion {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

  using hive::utilities::wif_to_key;

namespace detail {

  using hive::plugins::condenser_api::legacy_signed_transaction;
  using hive::plugins::condenser_api::legacy_signed_block;

  class node_based_conversion_plugin_impl final : public conversion_plugin_impl {
  public:

    node_based_conversion_plugin_impl( const std::string& input_url, const std::string& output_url,
      const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id,
      size_t signers_size, size_t block_buffer_size, bool use_now_time );

    void open( fc::http::connection& con, const fc::url& url );
    void close();

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;

    void transmit( const hp::signed_transaction& trx );

    fc::optional< hp::signed_block > receive_uncached( uint32_t num );
    fc::optional< hp::signed_block > receive( uint32_t block_num );

    const fc::variants& get_block_buffer()const;

    void validate_chain_id( const hp::chain_id_type& chain_id );
    uint32_t get_output_head_block_num();

    fc::http::connection input_con;
    fc::url              input_url;

    fc::http::connection output_con;
    fc::url              output_url;

    size_t               last_block_range_start = -1; // initial value to satisfy the `num < last_block_range_start` check to get new blocks on first call // TODO: Maybe initial value should be 0
    size_t               block_buffer_size;
    fc::variant_object   block_buffer_obj; // blocks buffer object containing lookup table

    bool use_now_time;
  };

  node_based_conversion_plugin_impl::node_based_conversion_plugin_impl( const std::string& input_url, const std::string& output_url,
    const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size, size_t block_buffer_size, bool use_now_time )
    : conversion_plugin_impl( _private_key, chain_id, signers_size ), input_url( input_url ), output_url( output_url ), block_buffer_size( block_buffer_size ), use_now_time( use_now_time )
  {
    FC_ASSERT( block_buffer_size && block_buffer_size <= 1000, "Blocks buffer size should be in the range 1-1000", ("block_buffer_size",block_buffer_size) );

    // Validate urls - must be in http://host:port format, where host can be either ipv4 address or domain name
    FC_ASSERT( this->input_url.proto() == "http" && this->output_url.proto() == "http",
      "Currently only http protocol is supported", ("in_proto",this->input_url.proto())("out_proto",this->output_url.proto()) );
    FC_ASSERT( this->input_url.host().valid() && this->output_url.host().valid(), "You have to specify the host in url", ("input_url",input_url)("output_url",output_url) );
    FC_ASSERT( this->input_url.port().valid() && this->output_url.port().valid(), "You have to specify the port in url", ("input_url",input_url)("output_url",output_url) );
  }

  // TODO: Support HTTP encrypted using TLS or SSL (HTTPS)
  void node_based_conversion_plugin_impl::open( fc::http::connection& con, const fc::url& url )
  {
    while( true )
    {
      try
      {
        con.connect_to( fc::resolve( *url.host(), *url.port() )[0] ); // First try to resolve the domain name
      }
      catch( const fc::exception& e )
      {
        try
        {
          con.connect_to( fc::ip::endpoint( *url.host(), *url.port() ) );
        } FC_CAPTURE_AND_RETHROW( (url) )
      }

      if (con.get_socket().is_open())
        break;

      else
      {
        wlog("Error connecting to server RPC endpoint, retrying in 1 second");
        fc::usleep(fc::seconds(1));
      }
    }
  }

  void node_based_conversion_plugin_impl::close()
  {
    if( input_con.get_socket().is_open() )
      input_con.get_socket().close();

    if( output_con.get_socket().is_open() )
      output_con.get_socket().close();

    if( !converter.has_hardfork( HIVE_HARDFORK_0_17__770 ) )
      std::cerr << "Conversion interrupted before HF17. Pow authorities can still be added into the blockchain. Resuming the conversion without the saved converter state will result in corrupted block log\n";
  }

  void node_based_conversion_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    if( !start_block_num )
      start_block_num = get_output_head_block_num() + 1;

    std::cout << "Continuing conversion from the block with number " << start_block_num
                << "\nValidating the chain id...\n";

    validate_chain_id( converter.get_chain_id() );
    std::cout << "Chain id match\n";

    fc::optional< hp::signed_block > block;

    hp::block_id_type last_block_id;
    while( start_block_num > 1 && !block.valid() )
    {
      if( appbase::app().is_interrupt_request() ) break;
      block = receive_uncached( start_block_num - 1 );
      if( block.valid() )
      {
        last_block_id = converter.convert_signed_block( *block, last_block_id );
      }
      else
      {
        std::cout << "Could not parse the block with number " << start_block_num << " from the host. Propably the block has not been produced yet. Retrying in 1 second...\n";
        fc::usleep(fc::seconds(1));
      }
    }

    for( ; ( start_block_num <= stop_block_num || !stop_block_num ) && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      block = receive( start_block_num );

      if( !block.valid() )
      {
        std::cout << "Could not parse the block with number " << start_block_num << " from the host. Propably the block has not been produced yet. Retrying in 1 second...\n";
        fc::usleep(fc::seconds(1));
        --start_block_num;
        continue;
      }

      if( start_block_num == 1 )
        conversion_plugin_impl::add_initminer_second_authority_trx_to_block( *block );

      if( start_block_num % 1000 == 0 ) // Progress
      {
        if( stop_block_num )
          std::cout << "[ " << int( float(start_block_num) / stop_block_num * 100 ) << "% ]: " << start_block_num << '/' << stop_block_num << " blocks rewritten.\r";
        else
          std::cout << start_block_num << " blocks rewritten.\r";
        std::cout.flush();
      }

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
      {
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "Rewritten block: " << start_block_num
          << ". Data before conversion: " << fc::json::to_string( v ) << '\n';
      }

      if( block->transactions.size() == 0 )
        continue; // Since we transmit only transactions, not entire blocks, we can skip block conversion if there are no transactions in the block

      last_block_id = converter.convert_signed_block( *block, last_block_id, use_now_time ? time_point_sec{ fc::time_point::now() } : blockchain_converter::auto_trx_time );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
      {
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "After conversion: " << fc::json::to_string( v ) << '\n';
      }

      for( const auto& trx : block->transactions )
        if( appbase::app().is_interrupt_request() ) // If there were multiple trxs in block user would have to wait for them to transmit before exiting without this check
          break;
        else
          transmit( trx );
    }

    if( !appbase::app().is_interrupt_request() )
      appbase::app().generate_interrupt_request();
  }

  void node_based_conversion_plugin_impl::transmit( const hp::signed_transaction& trx )
  {
    try
    {
      open( output_con, output_url );

      fc::variant v;
      fc::to_variant( legacy_signed_transaction( trx ), v );

      auto reply = output_con.request( "POST", output_url,
        "{\"jsonrpc\":\"2.0\",\"method\":\"condenser_api.broadcast_transaction\",\"params\":[" + fc::json::to_string( v ) + "],\"id\":1}"
        /*,{ { "Content-Type", "application/json" } } */
      ); // FIXME: asio.cpp:24 - Boost asio - Broken pipe error on localhost
      FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received after transmitting tx: ${id}", ("code", reply.status)("body", std::string(reply.body.begin(), reply.body.end()) ) );

// #define HIVE_CONVERTER_DEBUG_TRANSMIT // TODO: Remove before merge
#ifdef HIVE_CONVERTER_DEBUG_TRANSMIT
      std::string str_reply{ &*reply.body.begin(), reply.body.size() };
      fc::variant_object var_obj = fc::json::from_string( str_reply ).get_object();
      FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", str_reply) );
#endif

      output_con.get_socket().close();
    } FC_CAPTURE_AND_RETHROW( (trx.id().str()) )
  }

  const fc::variants& node_based_conversion_plugin_impl::get_block_buffer()const
  {
    return block_buffer_obj["result"].get_object()["blocks"].get_array();
  }

  fc::optional< hp::signed_block > node_based_conversion_plugin_impl::receive_uncached( uint32_t num )
  {
    std::cout << "Note: using uncached receive to save your computers memory on block " << num << '\r';
    block_buffer_obj = fc::variant_object{}; // Clear block_buffer_obj to free now redundant memory

    try
    {
      open( input_con, input_url );

      auto reply = input_con.request( "POST", input_url, // XXX: Move to block_api - fix deserialization
          "{\"jsonrpc\":\"2.0\",\"method\":\"condenser_api.get_block\",\"params\":[" + std::to_string( num ) + "],\"id\":1}"
          /*,{ { "Content-Type", "application/json" } } */
      );
      FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received when receiving block with number: ${num}", ("code", reply.status) );
      FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("code", reply.status) );

      std::string str_reply{ &*reply.body.begin(), reply.body.size() };
      fc::variant_object var_obj = fc::json::from_string( str_reply ).get_object();
      FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", str_reply) );

      if( !var_obj["result"].get_object().contains("previous") )
        return fc::optional< hp::signed_block >();

      input_con.get_socket().close();

      return var_obj["result"].template as< legacy_signed_block >();
    } FC_CAPTURE_AND_RETHROW( (num) )
  }

  fc::optional< hp::signed_block > node_based_conversion_plugin_impl::receive( uint32_t num )
  {
    size_t result_offset = num - last_block_range_start;

    if(    last_block_range_start != size_t(-1) // Initial value of last_block_range_start check (first receive call) - block_buffer_obj not initialized yet
        && ( result_offset + 1 > get_block_buffer().size() || result_offset + 1 == 0 ) ) // Not enough blocks cached (not enough blocks in blockchain so stop wasting time on caching)
      return receive_uncached( num );

    if(    num < last_block_range_start // Initial check ( when last_block_range_start is size_t(-1) )
        || num >= last_block_range_start + block_buffer_size // number out of buffered blocks range
      )
      try
      {
        open( input_con, input_url );

        last_block_range_start = block_buffer_size == 1 ? num : num - ( num % block_buffer_size == 0 ? block_buffer_size : num % block_buffer_size ) + 1;
        auto reply = input_con.request( "POST", input_url,
            "{\"jsonrpc\":\"2.0\",\"method\":\"block_api.get_block_range\",\"params\":{\"starting_block_num\":" + std::to_string( last_block_range_start ) + ",\"count\":" + std::to_string( block_buffer_size ) + "},\"id\":1}"
            /*,{ { "Content-Type", "application/json" } } */
        );
        FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received when receiving block with number: ${num}", ("code", reply.status) );
        // TODO: Move to boost to support chunked transfer encoding in responses
        FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("code", reply.status) );

        std::string str_reply{ &*reply.body.begin(), reply.body.size() };
        fc::variant_object var_obj = fc::json::from_string( str_reply ).get_object();
        FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", str_reply) );
        FC_ASSERT( var_obj["result"].get_object().contains("blocks"), "No blocks in JSON response", ("body", str_reply) );

        block_buffer_obj = var_obj;

        input_con.get_socket().close();
      } FC_CAPTURE_AND_RETHROW( (num) )

    const auto& block_buf = get_block_buffer();
    result_offset = num - last_block_range_start;

    if( result_offset + 1 > block_buf.size() || result_offset + 1 == 0 )
      return fc::optional< hp::signed_block >();

    return block_buf.at( result_offset ).template as< legacy_signed_block >();
  }

  void node_based_conversion_plugin_impl::validate_chain_id( const hp::chain_id_type& chain_id )
  {
    try
    {
      open( output_con, output_url );

      auto reply = output_con.request( "POST", output_url,
        "{\"jsonrpc\":\"2.0\",\"method\":\"database_api.get_config\",\"id\":1}"
        /*,{ { "Content-Type", "application/json" } } */
      );
      FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received after transmitting tx: ${id}", ("code", reply.status)("body", std::string(reply.body.begin(), reply.body.end()) ) );
      FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("code", reply.status) );

      std::string str_reply{ &*reply.body.begin(), reply.body.size() };
      fc::variant_object var_obj = fc::json::from_string( str_reply ).get_object();
      FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", str_reply) );
      FC_ASSERT( var_obj["result"].get_object().contains("HIVE_CHAIN_ID"), "No HIVE_CHAIN_ID in JSON response", ("body", str_reply) );

      const auto& chain_id_str = var_obj["result"].get_object()["HIVE_CHAIN_ID"].as_string();
      hp::chain_id_type remote_chain_id;

      try
      {
        remote_chain_id = hp::chain_id_type( chain_id_str );
      }
      catch( fc::exception& )
      {
        FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
      }

      FC_ASSERT( remote_chain_id == chain_id, "Remote chain id does not match the specified one", ("chain_id",chain_id)("remote_chain_id",remote_chain_id) );

      output_con.get_socket().close();
    } FC_CAPTURE_AND_RETHROW( (chain_id) )
  }

  uint32_t node_based_conversion_plugin_impl::get_output_head_block_num()
  {
    try
    {
      open( output_con, output_url );

      auto reply = output_con.request( "POST", output_url,
        "{\"jsonrpc\":\"2.0\",\"method\":\"database_api.get_dynamic_global_properties\",\"id\":1}"
        /*,{ { "Content-Type", "application/json" } } */
      );
      FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received after transmitting tx: ${id}", ("code", reply.status)("body", std::string(reply.body.begin(), reply.body.end()) ) );
      FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("code", reply.status) );

      std::string str_reply{ &*reply.body.begin(), reply.body.size() };
      fc::variant_object var_obj = fc::json::from_string( str_reply ).get_object();
      FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", str_reply) );
      FC_ASSERT( var_obj["result"].get_object().contains("head_block_number"), "No head_block_number in JSON response", ("body", str_reply) );

      output_con.get_socket().close();

      return var_obj["result"].get_object()["head_block_number"].as_uint64();
    } FC_CAPTURE_AND_RETHROW()
  }

} // detail

  node_based_conversion_plugin::node_based_conversion_plugin() {}
  node_based_conversion_plugin::~node_based_conversion_plugin() {}

  void node_based_conversion_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg )
  {
    cfg.add_options()
      ( "block-buffer-size", bpo::value< size_t >()->default_value( 1000 ), "Block buffer size" )
      ( "use-now-time,T", bpo::bool_switch()->default_value( false ), "Set expiration time of the transactions to the current system time (works only with the node_based_conversion plugin)" );
  }

  void node_based_conversion_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " plugin" );
    FC_ASSERT( options.count("output"), "You have to specify the output source for the " HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " plugin" );

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

    my = std::make_unique< detail::node_based_conversion_plugin_impl >(
          options.at( "input" ).as< std::string >(), options.at( "output" ).as< std::string >(),
          *private_key, _hive_chain_id, options.at( "jobs" ).as< size_t >(),
          options["block-buffer-size"].as< size_t >(), options["use-now-time"].as< bool >()
        );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );
  }

  void node_based_conversion_plugin::plugin_startup() {
    try
    {
      my->convert(
        appbase::app().get_args().at("resume-block").as< uint32_t >(),
        appbase::app().get_args().at( "stop-block" ).as< uint32_t >()
      );
    }
    catch( const fc::exception& e )
    {
      elog( e.to_detail_string() );
      appbase::app().generate_interrupt_request();
    }
  }
  void node_based_conversion_plugin::plugin_shutdown()
  {
    my->print_wifs();
    my->close();
  }

} } } } // hive::converter::plugins::block_log_conversion
