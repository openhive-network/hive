#include "node_based_conversion_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/network/tcp_socket.hpp>
#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>
#include <fc/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/rpc/http_api.hpp>
#include <fc/api.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/protocol/block.hpp>
#include <hive/utilities/key_conversion.hpp>
#include <hive/plugins/condenser_api/condenser_api_legacy_objects.hpp>

#include <boost/program_options.hpp>

#include <memory>

#include "conversion_plugin.hpp"

#include "converter.hpp"

namespace bpo = boost::program_options;

namespace hive { namespace converter { namespace plugins { namespace node_based_conversion {

using namespace hive::chain;
using namespace hive::utilities;
using namespace hive::protocol;
using namespace hive::plugins::condenser_api;

namespace detail {

  // XXX: This exists in p2p_plugin, http_plugin and node_based_conversion_plugin. It should be added to fc.
  std::vector<fc::ip::endpoint> resolve_string_to_ip_endpoints( const std::string& endpoint_string )
  {
    try
    {
      string::size_type colon_pos = endpoint_string.find( ':' );
      if( colon_pos == std::string::npos )
        FC_THROW( "Missing required port number in endpoint string \"${endpoint_string}\"",
              ("endpoint_string", endpoint_string) );

      std::string port_string = endpoint_string.substr( colon_pos + 1 );

      try
      {
        uint16_t port = boost::lexical_cast< uint16_t >( port_string );
        std::string hostname = endpoint_string.substr( 0, colon_pos );
        std::vector< fc::ip::endpoint > endpoints = fc::resolve( hostname, port );

        if( endpoints.empty() )
          FC_THROW_EXCEPTION( fc::unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname) );

        return endpoints;
      }
      catch( const boost::bad_lexical_cast& )
      {
        FC_THROW("Bad port: ${port}", ("port", port_string) );
      }
    }
    FC_CAPTURE_AND_RETHROW( (endpoint_string) )
  }

  class node_based_conversion_plugin_impl final : public conversion_plugin_impl {
  public:

    node_based_conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id );

    void open( const std::string& input_url, const std::string& output_url );
    void close();

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;

    void transmit( const signed_transaction& trx );

    template< size_t BlockBufferSize >
    signed_block receive( uint32_t block_num );

    fc::http::connection       input_con;
    fc::url                    input_url;

    fc::http::connection       output_con;
    fc::url                    output_url;
  };

  node_based_conversion_plugin_impl::node_based_conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID )
    : conversion_plugin_impl( _private_key, chain_id )
  {}

  void node_based_conversion_plugin_impl::open( const std::string& input_url, const std::string& output_url )
  {
    this->input_url = fc::url(input_url);
    this->output_url = fc::url(output_url);

    // TODO: Support HTTP encrypted using TLS or SSL (HTTPS)
    while( true )
    {
      try
      {
        input_con.connect_to( detail::resolve_string_to_ip_endpoints( *this->input_url.host() + ':' + std::to_string(*this->input_url.port()) )[0] );
      } FC_CAPTURE_AND_RETHROW( (input_url) )

      if (input_con.get_socket().is_open())
        break;

      else
      {
        wlog("Error connecting to server RPC endpoint (input), retrying in 10 seconds");
        fc::usleep(fc::seconds(10));
      }
    }

    while( true )
    {
      try
      {
        output_con.connect_to( fc::ip::endpoint::from_string( *this->output_url.host() + ':' + std::to_string(*this->output_url.port()) ) );
      } FC_CAPTURE_AND_RETHROW( (output_url) )

      if (output_con.get_socket().is_open())
        break;

      else
      {
        wlog("Error connecting to server RPC endpoint (output), retrying in 10 seconds");
        fc::usleep(fc::seconds(10));
      }
    }
  }

  void node_based_conversion_plugin_impl::close()
  {
    if( input_con.get_socket().is_open() )
      input_con.get_socket().close();

    if( output_con.get_socket().is_open() )
      output_con.get_socket().close();

    if( block_header::num_from_id( converter.get_previous_block_id() ) + 1 <= HIVE_HARDFORK_0_17_BLOCK_NUM )
      std::cerr << "Second authority has not been applied on the accounts yet! Try resuming the conversion process\n";
  }

  void node_based_conversion_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    FC_ASSERT( input_con.get_socket().is_open(), "Input connection socket should be opened before performing the conversion", ("input_url", input_url) );
    FC_ASSERT( output_con.get_socket().is_open(), "Output connection socket should be opened before performing the conversion", ("output_url", output_url) );

    if( !start_block_num )
      start_block_num = 1;

    block_id_type last_block_id{};
    signed_block block;

    for( ; ( start_block_num <= stop_block_num || !stop_block_num ) && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      try
      {
        // XXX: Allow users to change the BlockBufferSize template argument:
        block = receive< 1000 >( start_block_num );
      }
      catch(const fc::exception& e)
      {
        std::cout << "Retrying in 1 second...\n";
        fc::usleep(fc::seconds(1));
        --start_block_num;
        continue;
      }

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
        fc::to_variant( block, v );

        std::cout << "Rewritten block: " << start_block_num
          << ". Data before conversion: " << fc::json::to_string( v ) << '\n';
      }

      if( block.transactions.size() == 0 )
        continue; // Since we transmit only transactions, not entire blocks, we can skip block conversion if there are no transactions in the block

      last_block_id = converter.convert_signed_block( block, last_block_id );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
      {
        fc::variant v;
        fc::to_variant( block, v );

        std::cout << "After conversion: " << fc::json::to_string( v ) << '\n';
      }

      for( const auto& trx : block.transactions )
        transmit( trx );
    }

    if( !appbase::app().is_interrupt_request() )
      appbase::app().generate_interrupt_request();
  }

  void node_based_conversion_plugin_impl::transmit( const signed_transaction& trx )
  {
    try
    {
      fc::variant v;
      fc::to_variant( legacy_signed_transaction( trx ), v );

      // TODO: Remove this debug request body print in the production
      std::cout << "{\"jsonrpc\":\"2.0\",\"method\":\"condenser_api.broadcast_transaction\",\"params\":[" + fc::json::to_string( v ) + "],\"id\":1}\n";

      auto reply = output_con.request( "POST", output_url, // output_con.get_socket().remote_endpoint().operator string()
        "{\"jsonrpc\":\"2.0\",\"method\":\"condenser_api.broadcast_transaction\",\"params\":[" + fc::json::to_string( v ) + "],\"id\":1}"
        /*,{ { "Content-Type", "application/json" } } */
      ); // FIXME: Unknown transmit HTTP error
      FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received after transmitting tx: ${id}", ("id", trx.id().str())("code", reply.status)("body", std::string(reply.body.begin(), reply.body.end()) ) );
    }
    catch (const fc::exception& e)
    {
      elog("Caught exception while broadcasting tx ${id}:  ${e}", ("id", trx.id().str())("e", e.to_detail_string()) );
      throw;
    }
  }

  template< size_t BlocksBufferSize >
  signed_block node_based_conversion_plugin_impl::receive( uint32_t num )
  {
    FC_ASSERT( BlocksBufferSize && BlocksBufferSize <= 1000, "Blocks buffer size should be in the range 1-1000", ("BlocksBufferSize",BlocksBufferSize) );
    static size_t last_block_range_start = -1; // initial value to satisfy the `num < last_block_range_start` check to get new blocks on first call
    static std::array< fc::variant, BlocksBufferSize > blocks_buffer; // blocks lookup table

    if( num < last_block_range_start || num >= last_block_range_start + BlocksBufferSize )
      try
      {
        last_block_range_start = num - ( num % BlocksBufferSize ) + 1;
        auto reply = input_con.request( "POST", input_url,
            "{\"jsonrpc\":\"2.0\",\"method\":\"block_api.get_block_range\",\"params\":{\"starting_block_num\":" + std::to_string( last_block_range_start ) + ",\"count\":" + std::to_string(BlocksBufferSize) + "},\"id\":1}"
            /*,{ { "Content-Type", "application/json" } } */
        );
        FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received when receiving block with number: ${num}", ("num", num)("code", reply.status) );
        // TODO: Move to boost to support chunked transfer encoding in responses
        FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("num", num)("code", reply.status) );

        fc::variant_object var_obj = fc::json::from_string( &*reply.body.begin() ).get_object();
        FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", &*reply.body.begin()) );
        FC_ASSERT( var_obj["result"].get_object().contains("blocks"), "No blocks in JSON response", ("body", &*reply.body.begin()) );

        const auto& blocks = var_obj["result"].get_object()["blocks"].get_array();

        std::move( std::make_move_iterator( blocks.begin() ), std::make_move_iterator( blocks.end() ), blocks_buffer.begin() );
      }
      catch (const fc::exception& e)
      {
        elog("Caught exception while receiving block with number ${num}:  ${e}", ("num", num)("e", e.to_detail_string()) );
        throw;
      }

    return blocks_buffer.at( num - last_block_range_start ).template as< signed_block >();
  }

} // detail

  node_based_conversion_plugin::node_based_conversion_plugin() {}
  node_based_conversion_plugin::~node_based_conversion_plugin() {}

  void node_based_conversion_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg )
  {
    cfg.add_options()
      ( "blocks-buffer-size", bpo::value< size_t >()->default_value( 1000 ), "Blocks buffer size" );
  }

  void node_based_conversion_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " plugin" );
    FC_ASSERT( options.count("output"), "You have to specify the output source for the " HIVE_NODE_BASED_CONVERSION_PLUGIN_NAME " plugin" );

    chain_id_type _hive_chain_id;

    const auto& chain_id_str = options["chain-id"].as< std::string >();

    try
    {
      _hive_chain_id = chain_id_type( chain_id_str );
    }
    catch( fc::exception& )
    {
      FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
    }

    const auto private_key = wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse the private key" );

    my = std::make_unique< detail::node_based_conversion_plugin_impl >( *private_key, _hive_chain_id );

    my->set_logging( options["log-per-block"].as< uint32_t >(), options["log-specific"].as< uint32_t >() );

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );

    my->open( options.at( "input" ).as< std::string >(), options.at( "output" ).as< std::string >() );
  }

  void node_based_conversion_plugin::plugin_startup() {
    my->convert(
      appbase::app().get_args().at("resume-block").as< uint32_t >(),
      appbase::app().get_args().at( "stop-block" ).as< uint32_t >()
    );
  }
  void node_based_conversion_plugin::plugin_shutdown()
  {
    my->print_wifs();
    my->close();
  }

} } } } // hive::converter::plugins::block_log_conversion
