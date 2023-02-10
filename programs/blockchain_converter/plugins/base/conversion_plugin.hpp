#pragma once

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>

#include <fc/network/url.hpp>
#include <fc/network/http/connection.hpp>

#include <string>
#include <map>
#include <functional>

#include "../../converter.hpp"

// #define HIVE_CONVERTER_POST_DETAILED_LOGGING // Uncomment or define if you want to enable detailed logging along with the standard response message on error
// #define HIVE_CONVERTER_POST_SUPPRESS_WARNINGS // Uncomment or define if you want to suppress converter warnings
// #define HIVE_CONVERTER_POST_COUNT_ERRORS // Uncomment or define if you want to count error and their types and display them at the end

#ifdef HIVE_CONVERTER_POST_DETAILED_LOGGING
#  define HIVE_CONVERTER_POST_DETAILED_LOGGING_METHOD to_detail_string
#else
#  define HIVE_CONVERTER_POST_DETAILED_LOGGING_METHOD to_string
#endif

#define ICEBERG_GENERATE_CAPTURE_AND_LOG(...) catch( fc::exception& er ) { \
        wlog( "Caught an error during the conversion: \'${strerr}\'", ("strerr",er.HIVE_CONVERTER_POST_DETAILED_LOGGING_METHOD()) ); __VA_ARGS__; \
  } catch(...) { wlog( "Caught an unknown error during the conversion" ); __VA_ARGS__; }

namespace hive { namespace converter { namespace plugins {

  FC_DECLARE_EXCEPTION( error_response_from_node, 100000, "Got error response from the node while processing input block" );

  namespace hp = hive::protocol;
  using hp::chain_id_type;
  using hp::private_key_type;

  class conversion_plugin_impl {
  private:
    std::map<std::string, size_t> error_types;

  protected:
    blockchain_converter converter;

    conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id, appbase::application& app, size_t signers_size, bool increase_block_size = true );

    void handle_error_response_from_node( const error_response_from_node& error );

    void print_pre_conversion_data( const hp::signed_block& block_to_log )const;
    void print_progress( uint32_t current_block, uint32_t stop_block )const;
    void print_post_conversion_data( const hp::signed_block& block_to_log )const;

    void check_url( const fc::url& url )const;
    void display_error_response_data()const;

    void open( fc::http::connection& con, const fc::url& url );

    /**
     * @brief Sends a post request to the given hive endpoint with the prepared data
     *
     * @param con connection object to be used in order to estabilish connection with the server
     * @param url hive endpoint URL to connect to
     * @param method hive method to invoke
     * @param data data as a stringified JSON to be passed as a `param` property in the request, for example: `[0]`
     *
     * @return variant_object JSON object represented as a variant_object parsed from the `result` property from the reponse
     *
     * @throws fc::exception on connect error or `error` property present in the response
     */
    variant_object post( fc::http::connection& con, const fc::url& url, const std::string& method, const std::string& data );

    void transmit( const hc::full_transaction_type& trx, const fc::url& using_url );

    /**
     * @brief Get the dynamic global properties object as variant_object from the output node
     */
    fc::variant_object get_dynamic_global_properties( const fc::url& using_url );

    /**
     * @brief Get the id of the block preceding the one with the given number from the output node
     */
    hp::block_id_type get_previous_from_block( uint32_t num, const fc::url& using_url );

    void validate_chain_id( const hp::chain_id_type& chain_id, const fc::url& using_url );

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) = 0;

    uint64_t             error_response_count = 0;
    uint64_t             total_request_count = 0;

    std::function<void(const fc::variant_object&)> on_node_error_caught = [](const fc::variant_object&) {};

  public:
    /// If use_private is set to true then private keys of the second authority will be same as the given private key
    void set_wifs( bool use_private = false, const std::string& _owner = "", const std::string& _active = "", const std::string& _posting = "" );

    uint32_t log_per_block = 0, log_specific = 0;

    void print_wifs()const;

    const blockchain_converter& get_converter()const;
  };

} } } // hive::converter::plugins
