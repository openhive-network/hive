#include "conversion_plugin.hpp"

#include <fc/optional.hpp>
#include <fc/log/logger.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/variant.hpp>
#include <fc/variant_object.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>

#include <string>

namespace hive { namespace converter { namespace plugins {

  void conversion_plugin_impl::handle_error_response_from_node( const error_response_from_node& error )
  {
#ifndef HIVE_CONVERTER_POST_SUPPRESS_WARNINGS
# ifdef HIVE_CONVERTER_POST_DETAILED_LOGGING
    wlog( "${msg}", ("msg",error.to_detail_string()) );
# else
    wlog( "${msg}", ("msg",error.to_string()) );
# endif
#endif
  }

  using hive::protocol::private_key_type;
  using hive::protocol::authority;

  conversion_plugin_impl::conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id,
    appbase::application& app, size_t signers_size, bool increase_block_size )
    : converter( _private_key, chain_id, app, signers_size, increase_block_size ) {}

  void conversion_plugin_impl::check_url( const fc::url& url )const
  {
    FC_ASSERT( url.proto() == "http", "Currently only http protocol is supported", ("out_proto", url.proto()) );
    FC_ASSERT( url.host().valid(), "You have to specify the host in url", ("url",url) );
    FC_ASSERT( url.port().valid(), "You have to specify the port in url", ("url",url) );
  }

  void conversion_plugin_impl::display_error_response_data()const
  {
    if( total_request_count )
    {
#ifdef HIVE_CONVERTER_POST_COUNT_ERRORS
      wlog("Errors [count]: message");
      for( const auto& [key, val] : error_types )
        wlog(" > [${val}]: \t\"${key}\"", (val)(key));
#endif

      wlog("${errors} (${percent}% of total ${total}) node errors detected",
        ("errors", error_response_count)("percent", int(float(error_response_count) / total_request_count * 100))("total", total_request_count));
    }

    ilog("Processed total of ${total_tx_count} transactions", ("total_tx_count", converter.get_total_tx_count()));
  }

  void conversion_plugin_impl::open( fc::http::connection& con, const fc::url& url )
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

  void conversion_plugin_impl::transmit( const hc::full_transaction_type& trx, const fc::url& using_url )
  {
    try
    {
      fc::variant v;
      fc::to_variant( trx.get_transaction(), v );

      fc::http::connection local_output_con;
      post( local_output_con, using_url, "network_broadcast_api.broadcast_transaction", "{\"trx\":" + fc::json::to_string( v ) + "}" );
    }
    catch( const error_response_from_node& error )
    {
      handle_error_response_from_node( error );
    } FC_CAPTURE_AND_RETHROW( (trx.get_transaction_id().str()) )
  }

  hp::block_id_type conversion_plugin_impl::get_previous_from_block( uint32_t num, const fc::url& using_url )
  {
    try
    {
      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "block_api.get_block_header", "{\"block_num\":" + std::to_string( num ) + "}" );
      FC_ASSERT( var_obj.contains("header"), "No header in JSON response", ("reply", var_obj) );

      return var_obj["header"].get_object()["previous"].as< hp::block_id_type >();
    } // Note: we do not handle `error_response_from_node` as `get_previous_from_block` result is not required every time the function is called (usually every 1500ms)
    FC_CAPTURE_AND_RETHROW()
  }

  fc::variant_object conversion_plugin_impl::post( fc::http::connection& con, const fc::url& url, const std::string& method, const std::string& data )
  {
    try
    {
      ++total_request_count;

      open( con, url );

      auto reply = con.request( "POST", url,
          "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + data +  ",\"id\":1}"
          /*,{ { "Content-Type", "application/json" } } */
      );
      FC_ASSERT( reply.body.size(), "Reply body expected, but not received. Propably the server did not return the Content-Length header", ("code", reply.status) );
      std::string str_reply{ &*reply.body.begin(), reply.body.size() };

      FC_ASSERT( reply.status == fc::http::reply::OK, "HTTP 200 response code (OK) not received when sending request to the endpoint", ("code", reply.status)("reply", str_reply) );

      fc::variant_object var_obj = fc::json::from_string( str_reply, fc::json::format_validation_mode::full ).get_object();
      if( var_obj.contains( "error" ) )
      {
        const auto msg = var_obj["error"].get_object()["message"].get_string();

#ifdef HIVE_CONVERTER_POST_COUNT_ERRORS
        ++error_types[msg];
#endif

        try {
          this->on_node_error_caught( var_obj["error"].get_object() );
        } catch (...) {}

        FC_THROW_EXCEPTION( error_response_from_node, " ${block_num}: ${msg}",
                           ("msg", msg)
                           ("detailed",var_obj["error"].get_object())
                           ("block_num", hp::block_header::num_from_id(converter.get_mainnet_head_block_id()) + 1)
                          );
      }

      // By this point `result` should be present in the response
      FC_ASSERT( var_obj.contains( "result" ), "No result in JSON response", ("body", str_reply) );

      return var_obj["result"].get_object();
    }
    catch( const error_response_from_node& error )
    {
      ++error_response_count;

      throw error;
    } FC_CAPTURE_AND_RETHROW( (url)(data) )
  }

  void conversion_plugin_impl::validate_chain_id( const hp::chain_id_type& chain_id, const fc::url& using_url )
  {
    try
    {
      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "database_api.get_config", "{}" );

      FC_ASSERT( var_obj.contains("HIVE_CHAIN_ID"), "No HIVE_CHAIN_ID in JSON response", ("reply", var_obj) );

      const auto chain_id_str = var_obj["HIVE_CHAIN_ID"].as_string();
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
    } // Note: we do not handle `error_response_from_node` as `validate_chain_id` is called only once every program run and it is crucial for the converter to work
    FC_CAPTURE_AND_RETHROW( (chain_id) )
  }

  bool conversion_plugin_impl::account_exists( const fc::url& using_url, const hp::account_name_type& acc )
  {
    return accounts_exist(using_url, { acc });
  }

  bool conversion_plugin_impl::accounts_exist( const fc::url& using_url, const std::vector<hp::account_name_type>& accs )
  {
    try
    {
      fc::variant accounts_v;
      fc::to_variant(accs, accounts_v);

      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "database_api.find_accounts", "{\"accounts\":" + fc::json::to_string(accounts_v) + ",\"delayed_votes_active\":true}" );

      return var_obj.contains("accounts") && var_obj["accounts"].is_array() && var_obj["accounts"].get_array().size() == accs.size();
    }
    FC_CAPTURE_AND_RETHROW( (accs) )
  }

  bool conversion_plugin_impl::post_exists( const fc::url& using_url, const find_comments_pair_t& post )
  {
    return posts_exist(using_url, { post });
  }

  bool conversion_plugin_impl::posts_exist( const fc::url& using_url, const std::vector<find_comments_pair_t>& posts )
  {
    try
    {
      fc::variant accounts_v;
      fc::to_variant(posts, accounts_v);

      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "database_api.get_comment_pending_payouts", "{\"comments\":" + fc::json::to_string(accounts_v) + '}' );

      return var_obj.contains("cashout_infos") && var_obj["cashout_infos"].is_array() && var_obj["cashout_infos"].get_array().size() == posts.size();
    }
    FC_CAPTURE_AND_RETHROW( (posts) )
  }

  bool conversion_plugin_impl::transaction_applied( const fc::url& using_url, const hp::transaction_id_type& txid )
  {
    try
    {
      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "transaction_status_api.find_transaction", "{\"transaction_id\":\"" + txid.str() + "\"}" );

      if(var_obj.contains("status") && var_obj["status"].is_string())
      {
        const auto status = var_obj["status"].get_string();

        return status == "within_reversible_block" || status == "within_irreversible_block";
      }

      return false;
    }
    FC_CAPTURE_AND_RETHROW( (txid) )
  }

  fc::variant_object conversion_plugin_impl::get_dynamic_global_properties( const fc::url& using_url )
  {
    try
    {
      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "database_api.get_dynamic_global_properties", "{}" );
      // Check for example required property
      FC_ASSERT( var_obj.contains("head_block_number"), "No head_block_number in JSON response", ("reply", var_obj) );

      return var_obj;
    } // Note: we do not handle `error_response_from_node` as `get_dynamic_global_properties` result is not required every time the function is called (usually every 1500ms)
    FC_CAPTURE_AND_RETHROW()
  }

  std::vector< hp::account_name_type > conversion_plugin_impl::get_current_shuffled_witnesses( const fc::url& using_url )
  {
    try
    {
      auto var_obj = get_witness_schedule( using_url );
      FC_ASSERT( var_obj.contains("current_shuffled_witnesses"), "No current_shuffled_witnesses in JSON response", ("reply", var_obj) );

      std::vector< hp::account_name_type > witnesses = var_obj["current_shuffled_witnesses"].template as< std::vector< hp::account_name_type > >();

      std::vector< hp::account_name_type > output;

      // copy only existing witnesses ("" means there is no witness)
      std::copy_if (witnesses.begin(), witnesses.end(), std::back_inserter(output), [](const auto& wit){return wit.size() > 0;} );

      return output;
    }
    FC_CAPTURE_AND_RETHROW()
  }

  hp::asset conversion_plugin_impl::get_account_creation_fee( const fc::url& using_url )
  {
    try
    {
      auto var_obj = get_witness_schedule( using_url );
      FC_ASSERT( var_obj.contains("median_props"), "No median_props in JSON response", ("reply", var_obj) );

      return var_obj["median_props"]["account_creation_fee"].template as< hp::asset >();
    } // Note: we do not handle `error_response_from_node` as `get_dynamic_global_properties` result is not required every time the function is called (usually every 1500ms)
    FC_CAPTURE_AND_RETHROW()
  }

  fc::variant_object conversion_plugin_impl::get_witness_schedule( const fc::url& using_url )
  {
    try
    {
      fc::http::connection local_output_con;
      auto var_obj = post( local_output_con, using_url, "database_api.get_witness_schedule", "{}" );

      return var_obj;
    }
    FC_CAPTURE_AND_RETHROW()
  }

  void conversion_plugin_impl::print_pre_conversion_data( const hp::signed_block& block_to_log )const
  {
    uint32_t block_num = block_to_log.block_num();

    if ( ( log_per_block > 0 && block_num % log_per_block == 0 ) || log_specific == block_num )
      dlog("Processing block: ${block_num}. Data before conversion: ${block}", ("block_num", block_num)("block", block_to_log));
  }

  void conversion_plugin_impl::print_progress( uint32_t current_block, uint32_t stop_block )const
  {
    if( current_block % 1000 == 0 )
    { // Progress
      if( stop_block )
        ilog("[ ${progress}% ]: ${processed}/${stop_point} blocks rewritten",
          ("progress", int( float(current_block) / stop_block * 100 ))("processed", current_block)("stop_point", stop_block));
      else
        ilog("${block_num} blocks rewritten", ("block_num", current_block));
    }
  }

  void conversion_plugin_impl::print_post_conversion_data( const hp::signed_block& block_to_log )const
  {
    uint32_t block_num = block_to_log.block_num();

    if ( ( log_per_block > 0 && block_num % log_per_block == 0 ) || log_specific == block_num )
      dlog("Processing block: ${block_num}. Data after conversion: ${block}", ("block_num", block_num)("block", block_to_log));
  }

  void conversion_plugin_impl::set_wifs( bool use_private, const std::string& _owner, const std::string& _active, const std::string& _posting )
  {
    fc::optional< private_key_type > _owner_key = fc::ecc::private_key::wif_to_key( _owner );
    fc::optional< private_key_type > _active_key = fc::ecc::private_key::wif_to_key( _active );
    fc::optional< private_key_type > _posting_key = fc::ecc::private_key::wif_to_key( _posting );

    if( use_private )
      _owner_key = converter.get_witness_key();
    else if( !_owner_key.valid() )
      _owner_key = private_key_type::generate();

    if( use_private )
      _active_key = converter.get_witness_key();
    else if( !_active_key.valid() )
      _active_key = private_key_type::generate();

    if( use_private )
      _posting_key = converter.get_witness_key();
    else if( !_posting_key.valid() )
      _posting_key = private_key_type::generate();

    converter.set_second_authority_key( *_owner_key, authority::owner );
    converter.set_second_authority_key( *_active_key, authority::active );
    converter.set_second_authority_key( *_posting_key, authority::posting );

    converter.apply_second_authority_keys();
  }

  void conversion_plugin_impl::print_wifs()const
  {
    ilog( "Second authority wif private keys:" );
    dlog( "Owner: ${key}", ("key", converter.get_second_authority_key( authority::owner ).key_to_wif() ) );
    dlog( "Active: ${key}", ("key", converter.get_second_authority_key( authority::active ).key_to_wif() ) );
    dlog( "Posting: ${key}", ("key", converter.get_second_authority_key( authority::posting ).key_to_wif() ) );
  }

  const blockchain_converter& conversion_plugin_impl::get_converter()const
  {
    return converter;
  }

} } } // hive::converter::plugins
