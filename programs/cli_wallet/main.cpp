/*
  * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
  *
  * The MIT License
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE.
  */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <hive/chain/hive_fwd.hpp>

#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/http_api.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/smart_ref_impl.hpp>

#include <hive/utilities/git_revision.hpp>

#include <hive/protocol/protocol.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api.hpp>
#include <hive/wallet/misc_utilities.hpp>
#include <hive/wallet/wallet.hpp>

#include <fc/interprocess/signals.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>

#ifdef WIN32
# include <signal.h>
#else
# include <csignal>
#endif


using namespace hive::utilities;
using namespace hive::chain;
using namespace hive::wallet;
using namespace std;
namespace bpo = boost::program_options;

namespace
{
  fc::promise< int >::ptr exit_promise = new fc::promise<int>("cli_wallet exit promise");

  std::condition_variable rpc_server_connections;
  std::mutex rpc_server_mutex;
  std::atomic< uint64_t > rpc_server_transactions = { 0 };

  void sig_handler(int signal)
  {
    exit_promise->set_value(signal);
  }
}

int main( int argc, char** argv )
{
  try {
    const char* raw_password_from_environment = std::getenv("HIVE_WALLET_PASSWORD");
    std::string unlock_password_from_environment = raw_password_from_environment ? raw_password_from_environment : "";

    boost::program_options::options_description opts;
      opts.add_options()
      ("help,h", "Print this help message and exit.")
      ("version,v", "Print git revision sha of this cli_wallet build.")
      ("offline,o", "Run the wallet in offline mode.")
      ("server-rpc-endpoint,s", bpo::value<string>()->default_value("ws://127.0.0.1:8090"), "Server websocket RPC endpoint")
      ("cert-authority,a", bpo::value<string>()->default_value("_default"), "Trusted CA bundle file for connecting to wss:// TLS server")
      ("retry-server-connection", "Keep trying to connect to the Server websocket RPC endpoint if the first attempt fails")
      ("retry-server-connection-delay", bpo::value<uint16_t>()->default_value(10), "Delay [s] between every attempt of establishing a connection to the Server websocket RPC endpoint")
      ("rpc-endpoint,r", bpo::value<string>()->implicit_value("127.0.0.1:8091"), "Endpoint for wallet websocket RPC to listen on")
      ("rpc-tls-endpoint,t", bpo::value<string>()->implicit_value("127.0.0.1:8092"), "Endpoint for wallet websocket TLS RPC to listen on")
      ("rpc-tls-certificate,c", bpo::value<string>()->implicit_value("server.pem"), "PEM certificate for wallet websocket TLS RPC")
      ("rpc-http-endpoint,H", bpo::value<string>()->implicit_value("127.0.0.1:8093"), "Endpoint for wallet HTTP RPC to listen on")
      ("unlock", bpo::value<string>()->implicit_value(unlock_password_from_environment), "Password to automatically unlock wallet with "
                                                                                         "or use HIVE_WALLET_PASSWORD environment variable if no password is supplied")
      ("daemon,d", "Run the wallet in daemon mode" )
      ("rpc-http-allowip", bpo::value<vector<string>>()->multitoken(), "Allows only specified IPs to connect to the HTTP endpoint" )
      ("wallet-file,w", bpo::value<string>()->implicit_value("wallet.json"), "Wallet to load")
      ("chain-id", bpo::value< std::string >()->default_value( HIVE_CHAIN_ID ), "Chain ID to connect to")
      ("output-formatter", bpo::value< std::string >(), "Allows to present a result in different ways. Possible values(none/text/json)" )
      ("transaction-serialization", bpo::value< std::string >()->default_value( "legacy" ), "Allows to generate JSON using legacy/HF26 format. Possible values(legacy/hf26). By default is `legacy`." )
      ("store-transaction", bpo::value< std::string >(), "Requires a name of file. Allows to save a serialized transaction into the file in a current directory. By default a name of file is empty." )
      ;
    vector<string> allowed_ips;

    bpo::variables_map options;

    bpo::store( bpo::parse_command_line(argc, argv, opts), options );

    if( options.count("help") )
    {
      std::cout << opts << "\n";
      return 0;
    }
    if( options.count("version") )
    {
      std::cout << "hive_git_revision: " << std::string( hive::utilities::git_revision_sha ) << "\n";
      return 0;
    }
    if( options.count("rpc-http-allowip") && options.count("rpc-http-endpoint") ) {
      allowed_ips = options["rpc-http-allowip"].as<vector<string>>();
      wdump((allowed_ips));
    }

    hive::protocol::chain_id_type _hive_chain_id;

    if( options.count("chain-id") )
    {
      auto chain_id_str = options.at("chain-id").as< std::string >();

      try
      {
        _hive_chain_id = chain_id_type( chain_id_str);
      }
      catch( fc::exception& )
      {
        FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
      }
    }

#ifdef USE_ALTERNATE_CHAIN_ID
    std::cout << "STARTING "
# ifdef IS_TEST_NET
                            "TEST"
# else
                            "MIRROR"
# endif
                            " WALLET\n";
#else
    std::cout << "STARTING HIVE WALLET\n";
#endif
    std::cout << "chain id: " << std::string(_hive_chain_id) << "\n";
    std::cout << "blockchain version: " << fc::string(HIVE_BLOCKCHAIN_VERSION) << "\n";
    std::cout << "git_revision: \"" + fc::string(hive::utilities::git_revision_sha) + "\"\n";

    fc::path data_dir;
    fc::logging_config cfg;
    fc::path log_dir = data_dir / "logs";

    fc::file_appender::config ac;
    ac.filename             = log_dir / "rpc" / "rpc.log";
    ac.flush                = true;
    ac.rotate               = true;
    ac.rotation_interval    = fc::hours( 1 );
    ac.rotation_limit       = fc::days( 1 );

    std::cout << "Logging RPC to file: " << (data_dir / ac.filename).preferred_string() << "\n";

    cfg.appenders.push_back(fc::appender_config( "default", "console", fc::variant(fc::console_appender::config())));
    cfg.appenders.push_back(fc::appender_config( "rpc", "file", fc::variant(ac)));

    cfg.loggers = { fc::logger_config("default"), fc::logger_config( "rpc") };
    cfg.loggers.front().level = fc::log_level::info;
    cfg.loggers.front().appenders = {"default"};
    cfg.loggers.back().level = fc::log_level::debug;
    cfg.loggers.back().appenders = {"rpc"};


    //
    // TODO:  We read wallet_data twice, once in main() to grab the
    //    socket info, again in wallet_api when we do
    //    load_wallet_file().  Seems like this could be better
    //    designed.
    //
    wallet_data wdata;

    fc::path wallet_file( options.count("wallet-file") ? options.at("wallet-file").as<string>() : "wallet.json");
    if( fc::exists( wallet_file ) )
    {
      wdata = fc::json::from_file( wallet_file, fc::json::format_validation_mode::full ).as<wallet_data>();
    }
    else
    {
      std::cout << "Starting a new wallet\n";
    }

    // but allow CLI to override
    if( !options.at("server-rpc-endpoint").defaulted() )
      wdata.ws_server = options.at("server-rpc-endpoint").as<std::string>();

    // Override wallet data
    wdata.offline = options.count( "offline" );

    fc::http::websocket_client client( options["cert-authority"].as<std::string>() );
    idump((wdata.ws_server));
    fc::http::websocket_connection_ptr con;

    std::shared_ptr<wallet_api> wapiptr;
    boost::signals2::scoped_connection closed_connection;

    auto wallet_cli = std::make_shared<fc::rpc::cli>();

    auto get_output_formatter = []( const bpo::variables_map& options, output_formatter_type default_format )
    {
      if( options.count("output-formatter") )
      {
        fc::variant _v = options["output-formatter"].as<std::string>();
        output_formatter_type _result;
        from_variant( _v, _result );
        return _result;
      }
      else
      {
        return default_format;
      }
    };

    fc::variant _v = options["transaction-serialization"].as<std::string>();
    transaction_serialization_type _transaction_serialization;
    from_variant( _v, _transaction_serialization );

    std::string _store_transaction = options.count("store-transaction") ? options["store-transaction"].as<std::string>() : "";

    bool daemon_mode_enabled = options.count("daemon");

    if( !daemon_mode_enabled && ( options.count("rpc-endpoint") || options.count("rpc-tls-endpoint") || options.count("rpc-http-endpoint") ) )
    {
      ilog( "Daemon mode has been implicitly enabled by running the cli_wallet RPC server" );
      daemon_mode_enabled = true;
    }

    if( wdata.offline )
    {
      ilog( "Not connecting to server RPC endpoint, due to the offline option set" );
      wapiptr = std::make_shared<wallet_api>( wdata, _hive_chain_id, fc::api< hive::plugins::wallet_bridge_api::wallet_bridge_api >{}, exit_promise, daemon_mode_enabled, get_output_formatter( options, output_formatter_type::text ), _transaction_serialization, _store_transaction );
    }
    else
    {
      for (;;)
      {
        try
        {
          con = client.connect( wdata.ws_server );
        }
        catch (const fc::exception& e)
        {
          elog("Error during an attempt of connecting to a server RPC endpoint. ${error}", ("error", e.to_detail_string()));
          if (!options.count("retry-server-connection"))
          {
            ilog("Retrying a connection to a server RPC endpoint is not allowed");
            throw;
          }
        }
        if (con)
        {
          ilog("Connection to a server RPC endpoint is correctly done");
          break;
        }
        else
        {
          FC_ASSERT( options.count("retry-server-connection-delay") );
          auto _delay = options["retry-server-connection-delay"].as<uint16_t>();
          wlog("Error connecting to server RPC endpoint, retrying in ${_delay} seconds", (_delay));
          fc::usleep(fc::seconds(_delay));
        }
      }
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);
      auto remote_api = apic->get_remote_api< hive::plugins::wallet_bridge_api::wallet_bridge_api >(0, "wallet_bridge_api");

      output_formatter_type output_format;
      if( daemon_mode_enabled )
        output_format = get_output_formatter( options, output_formatter_type::none );
      else
        output_format = get_output_formatter( options, output_formatter_type::text );
      wapiptr = std::make_shared<wallet_api>( wdata, _hive_chain_id, remote_api, exit_promise, daemon_mode_enabled, output_format, _transaction_serialization, _store_transaction );
      closed_connection = con->closed.connect([=]{
        cerr << "Server has disconnected us.\n";
        wallet_cli->stop();
      });
      (void)(closed_connection);
    }

    wallet_cli->set_on_termination_handler( sig_handler );
    wapiptr->set_wallet_filename( wallet_file.generic_string() );

    fc::api<wallet_api> wapi(wapiptr);

    for( auto& name_formatter : wallet_formatter::get_result_formatters() )
      wallet_cli->format_result( name_formatter.first, name_formatter.second );

    if( wapiptr->is_new() )
    {
      std::cout << "Please use the set_password method to initialize a new wallet before continuing\n";
      wallet_cli->set_prompt( "new >>> " );
    } else
      wallet_cli->set_prompt( "locked >>> " );

    boost::signals2::scoped_connection locked_connection(wapiptr->lock_changed.connect([&](bool locked) {
      wallet_cli->set_prompt(  locked ? "locked >>> " : "unlocked >>> " );
    }));

    auto _websocket_server = std::make_shared<fc::http::websocket_server>();
    if( options.count("rpc-endpoint") )
    {
      _websocket_server->on_connection([&]( const fc::http::websocket_connection_ptr& c ){
        std::cout << "here... \n";
        wlog("." );
        auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(*c);
        wsc->register_api(wapi);
        c->set_session_data( wsc );
      });
      _websocket_server->on_message([&]( fc::future<void>& f ){
        ++rpc_server_transactions;
        f.wait();
        --rpc_server_transactions;
        rpc_server_connections.notify_all();
      });
      ilog( "Listening for incoming RPC requests on ${p}", ("p", options.at("rpc-endpoint").as<string>() ));
      _websocket_server->listen( fc::ip::endpoint::from_string(options.at("rpc-endpoint").as<string>()) );
      _websocket_server->start_accept();
    }

    string cert_pem = "server.pem";
    if( options.count( "rpc-tls-certificate" ) )
      cert_pem = options.at("rpc-tls-certificate").as<string>();

    auto _websocket_tls_server = std::make_shared<fc::http::websocket_tls_server>(cert_pem);
    if( options.count("rpc-tls-endpoint") )
    {
      _websocket_tls_server->on_connection([&]( const fc::http::websocket_connection_ptr& c ){
        auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(*c);
        wsc->register_api(wapi);
        c->set_session_data( wsc );
      });
      _websocket_tls_server->on_message([&]( fc::future<void>& f ){
        ++rpc_server_transactions;
        f.wait();
        --rpc_server_transactions;
        rpc_server_connections.notify_all();
      });
      ilog( "Listening for incoming TLS RPC requests on ${p}", ("p", options.at("rpc-tls-endpoint").as<string>() ));
      _websocket_tls_server->listen( fc::ip::endpoint::from_string(options.at("rpc-tls-endpoint").as<string>()) );
      _websocket_tls_server->start_accept();
    }

    set<fc::ip::address> allowed_ip_set;

    auto _http_server = std::make_shared<fc::http::server>();
    if( options.count("rpc-http-endpoint" ) )
    {
      for( const auto& ip : allowed_ips )
        allowed_ip_set.insert(fc::ip::address(ip));

      _http_server->listen( fc::ip::endpoint::from_string( options.at( "rpc-http-endpoint" ).as<string>() ) );
      ilog( "Listening for incoming HTTP RPC requests on ${endpoint}", ( "endpoint", _http_server->get_local_endpoint() ) );

      //
      // due to implementation, on_request() must come AFTER listen()
      //
      _http_server->on_request(
        [&]( const fc::http::request& req, const fc::http::server::response& resp )
        {
          ++rpc_server_transactions;

          if( allowed_ip_set.find( fc::ip::endpoint::from_string(req.remote_endpoint).get_address() ) == allowed_ip_set.end() &&
              allowed_ip_set.find( fc::ip::address() ) == allowed_ip_set.end() ) {
            elog("Rejected connection from ${ip} because it isn't in allowed set ${s}", ("ip",req.remote_endpoint)("s",allowed_ip_set) );
            resp.set_status( fc::http::reply::NotAuthorized );
            return;
          }
          std::shared_ptr< fc::rpc::http_base_api_connection > conn =
            std::make_shared< fc::rpc::http_base_api_connection>();
          conn->register_api( wapi );
          conn->on_request( req, resp );

          --rpc_server_transactions;
          rpc_server_connections.notify_all();
        } );
    }

    if( options.count("unlock" ) ) {
      wapi->unlock( options.at("unlock").as<string>() );
    }

    if( !daemon_mode_enabled )
    {
      wallet_cli->register_api( wapi );
      wallet_cli->start();
    }
    else
    {
      fc::set_signal_handler( sig_handler, SIGINT );
      fc::set_signal_handler( sig_handler, SIGTERM );
      ilog( "Entering Daemon Mode, ^C to exit" );
    }

    exit_promise->wait();
    {
      std::unique_lock< std::mutex > lk(rpc_server_mutex);
      // 0 or 1 (exit method invoked)
      rpc_server_connections.wait( lk, [&]{ return rpc_server_transactions <= 1; } );
    }
    wallet_cli->stop();

    wapi->save_wallet_file(wallet_file.generic_string());
    locked_connection.disconnect();
    closed_connection.disconnect();
  } FC_CAPTURE_AND_LOG(());

  return 0;
}
