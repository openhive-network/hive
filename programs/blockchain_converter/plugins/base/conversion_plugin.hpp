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

namespace hive { namespace converter { namespace plugins {

  FC_DECLARE_EXCEPTION( error_response_from_node, 100000, "Got error response from the node while processing input block" );

  namespace hp = hive::protocol;
  using hp::chain_id_type;
  using hp::private_key_type;

  using find_comments_pair_t = std::pair<hp::account_name_type, std::string>;

  class conversion_plugin_impl {
  private:
    std::map<std::string, size_t> error_types;

  protected:
    blockchain_converter converter;

    conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id, size_t signers_size, bool increase_block_size = true );

    void handle_error_response_from_node( const error_response_from_node& error );

    void display_error_response_data()const;

    void check_url( const fc::url& url )const;

    void print_pre_conversion_data( const hp::signed_block& block_to_log )const;
    void print_progress( uint32_t current_block, uint32_t stop_block )const;
    void print_post_conversion_data( const hp::signed_block& block_to_log )const;

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
    fc::variant_object post( fc::http::connection& con, const fc::url& url, const std::string& method, const std::string& data );

    void transmit( const hc::full_transaction_type& trx, const fc::url& using_url );

    /**
     * @brief Get the dynamic global properties object as variant_object from the output node
     */
    fc::variant_object get_dynamic_global_properties( const fc::url& using_url );

    /**
     * @brief Get account creation fee from the output node
     */
    hp::asset get_account_creation_fee( const fc::url& using_url );

    std::vector< hp::account_name_type > get_current_shuffled_witnesses( const fc::url& using_url );
    fc::variant_object get_witness_schedule( const fc::url& using_url );

    bool account_exists( const fc::url& using_url, const hp::account_name_type& acc );
    bool accounts_exist( const fc::url& using_url, const std::vector<hp::account_name_type>& accs );

    bool post_exists( const fc::url& using_url, const find_comments_pair_t& post );
    bool posts_exist( const fc::url& using_url, const std::vector<find_comments_pair_t>& posts );

    bool transaction_applied( const fc::url& using_url, const hp::transaction_id_type& txid );

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
