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

  class node_based_conversion_plugin_impl final : public conversion_plugin_impl {
  public:

    node_based_conversion_plugin_impl( const std::string& input_url, const std::string& output_url,
      const private_key_type& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID, size_t blocks_buffer_size = 1000 );

    void open( fc::http::connection& con, const fc::url& url );
    void close();

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;

    void transmit( const signed_transaction& trx );

    fc::optional< signed_block > receive( uint32_t block_num );

    const fc::variants& get_blocks_buffer()const;

    fc::http::connection input_con;
    fc::url              input_url;

    fc::http::connection output_con;
    fc::url              output_url;

    size_t               last_block_range_start = -1; // initial value to satisfy the `num < last_block_range_start` check to get new blocks on first call
    size_t               blocks_buffer_size;
    fc::variant_object   blocks_buffer_obj; // blocks buffer object containing lookup table
  };

  node_based_conversion_plugin_impl::node_based_conversion_plugin_impl( const std::string& input_url, const std::string& output_url,
    const private_key_type& _private_key, const chain_id_type& chain_id, size_t blocks_buffer_size )
    : conversion_plugin_impl( _private_key, chain_id ), input_url( input_url ), output_url( output_url ), blocks_buffer_size( blocks_buffer_size )
  {
    FC_ASSERT( blocks_buffer_size && blocks_buffer_size <= 1000, "Blocks buffer size should be in the range 1-1000", ("blocks_buffer_size",blocks_buffer_size) );

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

    if( block_header::num_from_id( converter.get_previous_block_id() ) + 1 <= HIVE_HARDFORK_0_17_BLOCK_NUM )
      std::cerr << "Second authority has not been applied on the accounts yet! Try resuming the conversion process\n";
  }

  void node_based_conversion_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    if( !start_block_num )
      start_block_num = 1;

    block_id_type last_block_id;
    fc::optional< signed_block > block;

    for( ; ( start_block_num <= stop_block_num || !stop_block_num ) && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      try
      {
        block = receive( start_block_num );
        if( !block.valid() ) throw "Invalid block";
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
        fc::to_variant( *block, v );

        std::cout << "Rewritten block: " << start_block_num
          << ". Data before conversion: " << fc::json::to_string( v ) << '\n';
      }

      if( block->transactions.size() == 0 )
        continue; // Since we transmit only transactions, not entire blocks, we can skip block conversion if there are no transactions in the block

      last_block_id = converter.convert_signed_block( *block, last_block_id );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
      {
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "After conversion: " << fc::json::to_string( v ) << '\n';
      }

      for( const auto& trx : block->transactions )
        transmit( trx );
    }

    if( !appbase::app().is_interrupt_request() )
      appbase::app().generate_interrupt_request();
  }

  void node_based_conversion_plugin_impl::transmit( const signed_transaction& trx )
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

      output_con.get_socket().close();
    } FC_CAPTURE_AND_RETHROW( (trx.id().str()) )
  }

  const fc::variants& node_based_conversion_plugin_impl::get_blocks_buffer()const
  {
    return blocks_buffer_obj["result"].get_object()["blocks"].get_array();
  }

  fc::optional< signed_block > node_based_conversion_plugin_impl::receive( uint32_t num )
  {
    if( num < last_block_range_start || num >= last_block_range_start + blocks_buffer_size )
      try
      {
        open( input_con, input_url );

        last_block_range_start = blocks_buffer_size == 1 ? num : num - ( num % blocks_buffer_size ) + 1;
        auto reply = input_con.request( "POST", input_url,
            "{\"jsonrpc\":\"2.0\",\"method\":\"block_api.get_block_range\",\"params\":{\"starting_block_num\":" + std::to_string( last_block_range_start ) + ",\"count\":" + std::to_string(blocks_buffer_size) + "},\"id\":1}"
            /*,{ { "Content-Type", "application/json" } } */
        );
        FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received when receiving block with number: ${num}", ("code", reply.status) );
        // TODO: Move to boost to support chunked transfer encoding in responses
        FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("code", reply.status) );

        fc::variant_object var_obj = fc::json::from_string( &*reply.body.begin() ).get_object();
        FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", &*reply.body.begin()) );
        FC_ASSERT( var_obj["result"].get_object().contains("blocks"), "No blocks in JSON response", ("body", &*reply.body.begin()) );

        blocks_buffer_obj = var_obj;

        input_con.get_socket().close();
      } FC_CAPTURE_AND_RETHROW( (num) )

    const auto& blocks_cache = get_blocks_buffer();

    if( num - last_block_range_start + 1 > blocks_cache.size() )
      return fc::optional< signed_block >();

    return blocks_cache.at( num - last_block_range_start ).template as< signed_block >();
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

    my = std::make_unique< detail::node_based_conversion_plugin_impl >(
          options.at( "input" ).as< std::string >(), options.at( "output" ).as< std::string >(),
          *private_key, _hive_chain_id, options["blocks-buffer-size"].as< size_t >()
        );

    my->set_logging( options["log-per-block"].as< uint32_t >(), options["log-specific"].as< uint32_t >() );

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
