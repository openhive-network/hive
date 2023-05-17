#pragma once

#include <appbase/application.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <hive/utilities/tempdir.hpp>

#include <hive/plugins/block_api/block_api_plugin.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>
#include <hive/plugins/chain/abstract_block_producer.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>

#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>

#include <array>
#include <iostream>

#define INITIAL_TEST_SUPPLY (10000000000ll)
#define HBD_INITIAL_TEST_SUPPLY (300000000ll)

extern uint32_t HIVE_TESTING_GENESIS_TIMESTAMP;

#define PUSH_TX \
  hive::chain::test::_push_transaction

#define PUSH_BLOCK \
  hive::chain::test::_push_block

#define GENERATE_BLOCK \
  hive::chain::test::_generate_block

// See below
#define REQUIRE_OP_VALIDATION_SUCCESS( op, field, value ) \
{ \
  const auto temp = op.field; \
  op.field = value; \
  op.validate(); \
  op.field = temp; \
}
#define REQUIRE_OP_EVALUATION_SUCCESS( op, field, value ) \
{ \
  const auto temp = op.field; \
  op.field = value; \
  trx.operations.back() = op; \
  op.field = temp; \
  push_transaction( trx, fc::ecc::private_key(), ~0 ); \
}

/*#define HIVE_REQUIRE_THROW( expr, exc_type )            \
{                                                         \
  std::string req_throw_info = fc::json::to_string(      \
    fc::mutable_variant_object()                        \
    ("source_file", __FILE__)                           \
    ("source_lineno", __LINE__)                         \
    ("expr", #expr)                                     \
    ("exc_type", #exc_type)                             \
    );                                                  \
  if( fc::enable_record_assert_trip )                    \
    std::cout << "HIVE_REQUIRE_THROW begin "            \
      << req_throw_info << std::endl;                  \
  BOOST_REQUIRE_THROW( expr, exc_type );                 \
  if( fc::enable_record_assert_trip )                    \
    std::cout << "HIVE_REQUIRE_THROW end "              \
      << req_throw_info << std::endl;                  \
}*/

#define HIVE_REQUIRE_THROW( expr, exc_type )           \
  BOOST_REQUIRE_THROW( expr, exc_type );

#define HIVE_CHECK_THROW( expr, exc_type )             \
{                                                      \
  std::string req_throw_info = fc::json::to_string(    \
    fc::mutable_variant_object()                       \
    ("source_file", __FILE__)                          \
    ("source_lineno", __LINE__)                        \
    ("expr", #expr)                                    \
    ("exc_type", #exc_type)                            \
    );                                                 \
  if( fc::enable_record_assert_trip )                  \
    std::cout << "HIVE_CHECK_THROW begin "             \
      << req_throw_info << std::endl;                  \
  BOOST_CHECK_THROW( expr, exc_type );                 \
  if( fc::enable_record_assert_trip )                  \
    std::cout << "HIVE_CHECK_THROW end "               \
      << req_throw_info << std::endl;                  \
}

#define HIVE_REQUIRE_EXCEPTION( expr, assert_test, ex_type )      \
do {                                                              \
  bool flag = fc::enable_record_assert_trip;                      \
  fc::enable_record_assert_trip = true;                           \
  HIVE_REQUIRE_THROW( expr, ex_type );                            \
  BOOST_REQUIRE_EQUAL( fc::last_assert_expression, assert_test ); \
  fc::enable_record_assert_trip = flag;                           \
} while(false)

#define HIVE_REQUIRE_ASSERT( expr, assert_test ) HIVE_REQUIRE_EXCEPTION( expr, assert_test, fc::assert_exception )
#define HIVE_REQUIRE_CHAINBASE_ASSERT( expr, assert_msg ) HIVE_REQUIRE_EXCEPTION( expr, assert_msg, fc::exception )

#define REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, exc_type ) \
{ \
  const auto temp = op.field; \
  op.field = value; \
  HIVE_REQUIRE_THROW( op.validate(), exc_type ); \
  op.field = temp; \
}
#define REQUIRE_OP_VALIDATION_FAILURE( op, field, value ) \
  REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, fc::exception )

#define REQUIRE_THROW_WITH_VALUE_2(op, field, value, exc_type) \
{ \
  auto bak = op.field; \
  op.field = value; \
  trx.operations.back() = op; \
  op.field = bak; \
  HIVE_REQUIRE_THROW(push_transaction(trx, ~0), exc_type); \
}

#define REQUIRE_THROW_WITH_VALUE( op, field, value ) \
  REQUIRE_THROW_WITH_VALUE_2( op, field, value, fc::exception )

///This simply resets v back to its default-constructed value. Requires v to have a working assignment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((struct test*)this)->test_method(); trx.clear()

#define PREP_ACTOR(name) \
  fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name));   \
  fc::ecc::private_key name ## _post_key = generate_private_key(std::string( BOOST_PP_STRINGIZE(name) ) + "_post" ); \
  public_key_type name ## _public_key = name ## _private_key.get_public_key();

#define ACTOR(name) \
  PREP_ACTOR(name) \
  const auto& name = account_create(BOOST_PP_STRINGIZE(name), name ## _public_key, name ## _post_key.get_public_key()); \
  account_id_type name ## _id = name.get_id(); (void)name ## _id;

#define ACTORS_IMPL(r, data, elem) ACTOR(elem)
#define ACTORS(names) BOOST_PP_SEQ_FOR_EACH(ACTORS_IMPL, ~, names) \
  validate_database();


// Generate accounts with the account creation fee instead of vesting 100 hive
#define ACTOR_DEFAULT_FEE(name) \
  PREP_ACTOR(name) \
  const auto& name = account_create_default_fee(BOOST_PP_STRINGIZE(name), name ## _public_key, name ## _post_key.get_public_key()); \
  account_id_type name ## _id = name.get_id(); (void)name ## _id;

#define ACTORS_DEFAULT_FEE_IMPL(r, data, elem) ACTOR_DEFAULT_FEE(elem)
#define ACTORS_DEFAULT_FEE(names) BOOST_PP_SEQ_FOR_EACH(ACTORS_DEFAULT_FEE_IMPL, ~, names) \
  validate_database();

#define PREP_ACTOR_EXT(object, name) \
  fc::ecc::private_key name ## _private_key = object.generate_private_key(BOOST_PP_STRINGIZE(name));   \
  fc::ecc::private_key name ## _post_key = object.generate_private_key(std::string( BOOST_PP_STRINGIZE(name) ) + "_post" ); \
  public_key_type name ## _public_key = name ## _private_key.get_public_key();

#define ACTOR_EXT(object, name) \
  PREP_ACTOR_EXT(object, name) \
  const auto& name = object.account_create(BOOST_PP_STRINGIZE(name), name ## _public_key, name ## _post_key.get_public_key()); \
  account_id_type name ## _id = name.get_id(); (void)name ## _id;

#define ACTORS_EXT_IMPL(r, data, elem) ACTOR_EXT(data, elem)
#define ACTORS_EXT(object, names) BOOST_PP_SEQ_FOR_EACH(ACTORS_EXT_IMPL, object, names) \
  object.validate_database();

#define SMT_SYMBOL( name, decimal_places, db ) \
  asset_symbol_type name ## _symbol = get_new_smt_symbol( decimal_places, db );

#define ASSET( s ) \
  hive::protocol::legacy_asset::from_string( s ).to_asset()

#define FUND( account_name, amount ) \
  fund( account_name, amount ); \
  generate_block();

// To be incorporated into fund() method if deemed appropriate.
// 'SMT' would be dropped from the name then.
#define FUND_SMT_REWARDS( account_name, amount ) \
  db->adjust_reward_balance( account_name, amount ); \
  db->adjust_supply( amount ); \
  generate_block();

#define OP2TX(OP,TX) \
TX.operations.push_back( OP ); \
TX.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

#define PUSH_OP(OP,KEY) \
{ \
  signed_transaction tx; \
  OP2TX(OP,tx) \
  push_transaction( tx, KEY, 0, hive::protocol::serialization_mode_controller::get_current_pack(), fc::ecc::bip_0062 ); \
}

#define PUSH_OP_TWICE(OP,KEY) \
{ \
  signed_transaction tx; \
  OP2TX(OP,tx) \
  push_transaction( tx, KEY, 0, hive::protocol::serialization_mode_controller::get_current_pack(), fc::ecc::bip_0062 ); \
  push_transaction( tx, KEY, database::skip_transaction_dupe_check, hive::protocol::serialization_mode_controller::get_current_pack(), fc::ecc::bip_0062 ); \
}

#define FAIL_WITH_OP(OP,KEY,EXCEPTION) \
{ \
  signed_transaction tx; \
  OP2TX(OP,tx) \
  HIVE_REQUIRE_THROW( push_transaction( tx, KEY, 0, hive::protocol::serialization_mode_controller::get_current_pack(), fc::ecc::bip_0062 ), EXCEPTION ); \
}

namespace hive { namespace chain {

using namespace hive::protocol;

struct autoscope
{
  std::function< void() > finalizer;
  autoscope( const std::function< void() >& f ) : finalizer( f ) {}
  ~autoscope() { finalizer(); }
};
//applies HF25 values related to comment cashout instead of those default (short) for testnet
//call configuration_data.reset_cashout_values() manually at the end of test (f.e. in fixture destructor) if
//you pass false to auto_reset, otherwise the setting will persist influencing other tests
autoscope set_mainnet_cashout_values( bool auto_reset = true );

//applies HF25 values related to witness feed instead of those default (short) for testnet
//call configuration_data.reset_feed_values() manually at the end of test (f.e. in fixture destructor) if
//you pass false to auto_reset, otherwise the setting will persist influencing other tests
autoscope set_mainnet_feed_values( bool auto_reset = true );

//common code for preparing arguments and data path
//caller needs to register plugins and call initialize() on given application object
//returned path should be passed as open_database() call parameter
fc::path common_init( const std::function< void( appbase::application& app, int argc, char** argv ) >& app_initializer );

struct database_fixture {
  // the reason we use an app is to exercise the indexes of built-in
  //   plugins
  chain::database* db = nullptr;
  signed_transaction trx;
  public_key_type committee_key;
  account_id_type committee_account;
  fc::ecc::private_key private_key = fc::ecc::private_key::generate();
  fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
  string debug_key = init_account_priv_key.key_to_wif();
  public_key_type init_account_pub_key = init_account_priv_key.get_public_key();
  uint32_t default_skip = 0 | database::skip_undo_history_check | database::skip_authority_check;
  fc::ecc::canonical_signature_type default_sig_canon = fc::ecc::fc_canonical;

  typedef plugins::account_history_rocksdb::account_history_rocksdb_plugin ah_plugin_type;
  ah_plugin_type* ah_plugin = nullptr;
  plugins::debug_node::debug_node_plugin* db_plugin = nullptr;
  plugins::rc::rc_plugin* rc_plugin = nullptr;

  optional<fc::temp_directory> data_dir;
  bool skip_key_index_test = false;

  database_fixture() {}
  virtual ~database_fixture() { appbase::reset(); }

  static fc::ecc::private_key generate_private_key( string seed = "init_key" );
#ifdef HIVE_ENABLE_SMT
  static asset_symbol_type get_new_smt_symbol( uint8_t token_decimal_places, chain::database* db );
#endif

  static const uint16_t shared_file_size_in_mb_64 = 64;
  static const uint16_t shared_file_size_in_mb_512 = 512;

  void open_database( const fc::path& _data_dir,
                      uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 );
  void generate_block(uint32_t skip = 0,
                      const fc::ecc::private_key& key = generate_private_key("init_key"),
                      int miss_blocks = 0);

  uint32_t generate_blocks( const std::string& debug_key, uint32_t count, uint32_t skip );

  uint32_t generate_blocks_until( const std::string& debug_key, const fc::time_point_sec& head_block_time,
    bool generate_sparsely, uint32_t skip );

  /**
    * @brief Returns number of the last irreversible block
    */
  uint32_t get_last_irreversible_block_num();

  /**
    * @brief Generates block_count blocks
    * @param block_count number of blocks to generate
    */
  void generate_blocks(uint32_t block_count);

  /**
    * @brief Generates blocks until the head block time matches or exceeds timestamp
    * @param timestamp target time to generate blocks until
    */
  void generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks = true);

  void generate_seconds_blocks( uint32_t seconds, bool skip_interm_blocks = true );
  void generate_days_blocks( uint32_t days, bool skip_interm_blocks = true );
  void generate_until_block( uint32_t block_num );
  void generate_until_irreversible_block( uint32_t block_num );

  fc::string get_current_time_iso_string() const;

  const account_object& account_create(
    const string& name,
    const string& creator,
    const private_key_type& creator_key,
    const share_type& fee,
    const public_key_type& key,
    const public_key_type& post_key,
    const string& json_metadata
  );

  const account_object& account_create(
    const string& name,
    const public_key_type& key,
    const public_key_type& post_key
  );

  const account_object& account_create_default_fee(
    const string& name,
    const public_key_type& key,
    const public_key_type& post_key
  );

  const account_object& account_create(
    const string& name,
    const public_key_type& key
  );

  const witness_object& witness_create(
    const string& owner,
    const private_key_type& owner_key,
    const string& url,
    const public_key_type& signing_key,
    const share_type& fee
  );

  void account_update( const string& account, const fc::ecc::public_key& memo_key, const string& metadata,
                       optional<authority> owner, optional<authority> active, optional<authority> posting,
                       const fc::ecc::private_key& key );
  void account_update2( const string& account, optional<authority> owner, optional<authority> active, optional<authority> posting,
                        optional<public_key_type> memo_key, const string& metadata, const string& posting_metadata,
                        const fc::ecc::private_key& key );

  void push_transaction( const operation& op, const fc::ecc::private_key& key );
  full_transaction_ptr push_transaction( const signed_transaction& tx, const fc::ecc::private_key& key = fc::ecc::private_key(),
    uint32_t skip_flags = 0, hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack(),
    fc::ecc::canonical_signature_type _sig_type = fc::ecc::fc_canonical );
  full_transaction_ptr push_transaction( const signed_transaction& tx, const std::vector<fc::ecc::private_key>& keys,
    uint32_t skip_flags = 0, hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack(),
    fc::ecc::canonical_signature_type _sig_type = fc::ecc::fc_canonical );

  bool push_block( const std::shared_ptr<full_block_type>& b, uint32_t skip_flags = 0 );

  void fund( const string& account_name, const share_type& amount = 500000 );
  void fund( const string& account_name, const asset& amount, bool update_print_rate = true );
  void transfer( const string& from, const string& to, const asset& amount );
  void transfer( const string& from, const string& to, const asset& amount, const std::string& memo, const fc::ecc::private_key& key );
  void recurrent_transfer( const string& from, const string& to, const asset& amount, const string& memo, uint16_t recurrence,
                           uint16_t executions, const fc::ecc::private_key& key );
  void convert( const string& account_name, const asset& amount );
  void convert_hbd_to_hive( const std::string& owner, uint32_t requestid, const asset& amount, const fc::ecc::private_key& key );
  void collateralized_convert_hive_to_hbd( const std::string& owner, uint32_t requestid, const asset& amount, const fc::ecc::private_key& key );
  void vest( const string& from, const string& to, const asset& amount );
  void vest( const string& from, const share_type& amount );
  void vest( const string& from, const string& to, const asset& amount, const fc::ecc::private_key& key );
  void delegate_vest( const string& delegator, const string& delegatee, const asset& amount, const fc::ecc::private_key& key );
  void set_withdraw_vesting_route(const string& from, const string& to, uint16_t percent, bool auto_vest, const fc::ecc::private_key& key);
  void withdraw_vesting( const string& account, const asset& amount, const fc::ecc::private_key& key );
  void proxy( const string& account, const string& proxy );
  void proxy( account_name_type _account, account_name_type _proxy, const fc::ecc::private_key& _key );
  void set_price_feed( const price& new_price, bool stop_at_update_block = false );
  void set_witness_props( const flat_map< string, vector< char > >& new_props );
  void witness_feed_publish( const string& publisher, const price& exchange_rate, const private_key_type& key );
  void witness_vote( account_name_type voter, account_name_type witness, const fc::ecc::private_key& key, bool approve = true );
  void limit_order_create( const string& owner, const asset& amount_to_sell, const asset& min_to_receive, bool fill_or_kill,
                           const fc::microseconds& expiration_shift, uint32_t orderid, const fc::ecc::private_key& key);
  void limit_order_cancel( const string& owner, uint32_t orderid, const fc::ecc::private_key& key );
  void limit_order2_create( const string& owner, const asset& amount_to_sell, const price& exchange_rate, bool fill_or_kill,
                            const fc::microseconds& expiration_shift, uint32_t orderid, const fc::ecc::private_key& key );
  void escrow_transfer( const string& from, const string& to, const string& agent, const asset& hive_amount, 
                        const asset& hbd_amount, const asset& fee, const std::string& json_meta, const fc::microseconds& ratification_shift,
                        const fc::microseconds& expiration_shift, uint32_t escrow_id, const fc::ecc::private_key& key );
  void escrow_approve( const string& from, const string& to, const string& agent, const string& who, bool approve, uint32_t escrow_id, const fc::ecc::private_key& key );
  void escrow_release( const string& from, const string& to, const string& agent, const string& who, const string& receiver,
                       const asset& hive_amount, const asset& hbd_amount, uint32_t escrow_id, const fc::ecc::private_key& key );
  void escrow_dispute( const string& from, const string& to, const string& agent, const string& who, uint32_t escrow_id, const fc::ecc::private_key& key );
  void transfer_to_savings( const string& from, const string& to, const asset& amount, const string& memo, const fc::ecc::private_key& key );
  void transfer_from_savings( const string& from, const string& to, const asset& amount, uint32_t request_id,
                              const fc::ecc::private_key& key );
  void cancel_transfer_from_savings( const string& from, uint32_t request_id, const fc::ecc::private_key& key );

  void push_custom_operation( const flat_set< account_name_type >& required_auths, uint16_t id,
                              const vector< char >& data, const fc::ecc::private_key& key );
  void push_custom_json_operation( const flat_set< account_name_type >& required_auths, 
                                   const flat_set< account_name_type >& required_posting_auths,
                                   const custom_id_type& id, const std::string& json, const fc::ecc::private_key& key );

  void decline_voting_rights( const string& account, const bool decline, const fc::ecc::private_key& key );

  struct create_proposal_data
  {
    std::string creator    ;
    std::string receiver   ;
    fc::time_point_sec start_date ;
    fc::time_point_sec end_date   ;
    hive::protocol::asset daily_pay ;
    std::string subject ;
    std::string url     ;

    create_proposal_data(fc::time_point_sec _start)
    {
      creator    = "alice";
      receiver   = "bob";
      start_date = _start     + fc::days( 1 );
      end_date   = start_date + fc::days( 2 );
      daily_pay  = asset( 100, HBD_SYMBOL );
      subject    = "hello";
      url        = "http:://something.html";
    }
  };

  int64_t create_proposal( const std::string& creator, const std::string& receiver, const std::string& subject, const std::string& permlink,
                           time_point_sec start_date, time_point_sec end_date, asset daily_pay, const fc::ecc::private_key& key );
  int64_t create_proposal(   std::string creator, std::string receiver,
                    time_point_sec start_date, time_point_sec end_date,
                    asset daily_pay, const fc::ecc::private_key& key, bool with_block_generation = true );
  void vote_proposal( std::string voter, const std::vector< int64_t >& id_proposals, bool approve, const fc::ecc::private_key& key );
  void remove_proposal(account_name_type _deleter, flat_set<int64_t> _proposal_id, const fc::ecc::private_key& _key);
  void update_proposal(uint64_t proposal_id, std::string creator, asset daily_pay, std::string subject, std::string permlink, const fc::ecc::private_key& key, time_point_sec* end_date = nullptr );

  bool exist_proposal( int64_t id );
  const proposal_object* find_proposal( int64_t id );
  const proposal_vote_object* find_vote_for_proposal(const std::string& _user, int64_t _proposal_id);
  uint64_t get_nr_blocks_until_proposal_maintenance_block();
  uint64_t get_nr_blocks_until_daily_proposal_maintenance_block();

  account_id_type get_account_id( const string& account_name )const;
  asset get_balance( const string& account_name )const;
  asset get_hbd_balance( const string& account_name )const;
  asset get_savings( const string& account_name )const;
  asset get_hbd_savings( const string& account_name )const;
  asset get_rewards( const string& account_name )const;
  asset get_hbd_rewards( const string& account_name )const;
  asset get_vesting( const string& account_name )const;
  asset get_vest_rewards( const string& account_name )const;
  asset get_vest_rewards_as_hive( const string& account_name )const;

private:

  void post_comment_internal( const std::string& _author, const std::string& _permlink, const std::string& _title, const std::string& _body,
                              const std::string& _parent_author, const std::string& _parent_permlink, const fc::ecc::private_key& _key );

public:

  void post_comment_with_block_generation( std::string _author, std::string _permlink, std::string _title, std::string _body, std::string _parent_permlink, const fc::ecc::private_key& _key );
  void post_comment( std::string _author, std::string _permlink, std::string _title, std::string _body, std::string _parent_permlink, const fc::ecc::private_key& _key);
  void post_comment_to_comment( const std::string& author, const std::string& permlink, const std::string& title, const std::string& body,
                                const std::string& parent_author, const std::string& parent_permlink, const fc::ecc::private_key& key );
  void delete_comment( std::string _author, std::string _permlink, const fc::ecc::private_key& _key );
  void set_comment_options( const std::string& author, const std::string& permlink, const asset& max_accepted_payout, uint16_t percent_hbd,
                            bool allow_curation_rewards, bool allow_votes, const fc::ecc::private_key& key );
  void set_comment_options( const std::string& author, const std::string& permlink, const asset& max_accepted_payout, uint16_t percent_hbd,
                            bool allow_curation_rewards, bool allow_votes, const comment_options_extensions_type& extensions, const fc::ecc::private_key& key );
  void vote( std::string _author, std::string _permlink, std::string _voter, int16_t _weight, const fc::ecc::private_key& _key );
  void claim_reward_balance( const std::string& account, const asset& reward_hive, const asset& reward_hbd, const asset& reward_vests,
                             const fc::ecc::private_key& key );
  /// @brief Creates proof of work and account for the worker. Also posts a comment by initminer for reasons explained in the body.
  /// @param _name Name of the worker (and account to be created too).
  /// @param _public_key worker (account) public key
  /// @param _private_key worker (account) private key
  void create_with_pow( std::string _name, const fc::ecc::public_key& _public_key, const fc::ecc::private_key& _private_key );
  /// Same as create_with_pow but uses pow2 operation and does a transfer instead of comment (see comments in body).
  void create_with_pow2( std::string _name, const fc::ecc::public_key& _public_key, const fc::ecc::private_key& _private_key );
  void create_with_delegation( const std::string& creator, const std::string& new_account_name, const fc::ecc::public_key& public_key,
                               const fc::ecc::private_key& posting_key, const asset& delegation, const fc::ecc::private_key& key );
  void claim_account( const std::string& creator, const asset& fee, const fc::ecc::private_key& key );
  void create_claimed_account( const std::string& creator, const std::string& new_account_name, const fc::ecc::public_key& public_key,
                               const fc::ecc::public_key& posting_key, const string& json_metadata, const fc::ecc::private_key& key );
  void change_recovery_account( const std::string& account_to_recover, const std::string& new_recovery_account, const fc::ecc::private_key& key );
  void request_account_recovery( const std::string& recovery_account, const std::string& account_to_recover, 
                                 const authority& new_owner_authority, const fc::ecc::private_key& key );
  void recover_account( const std::string& account_to_recover, const fc::ecc::private_key& new_owner_key, 
                        const fc::ecc::private_key& recent_owner_key );

  vector< operation > get_last_operations( uint32_t ops );

  void validate_database();
};

struct clean_database_fixture : public database_fixture
{
  clean_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_512, fc::optional<uint32_t> hardfork = fc::optional<uint32_t>() );
  virtual ~clean_database_fixture();

  void resize_shared_mem( uint64_t size, fc::optional<uint32_t> hardfork = fc::optional<uint32_t>() );
  void validate_database();
  void inject_hardfork( uint32_t hardfork );
};

struct hardfork_database_fixture : public clean_database_fixture
{
  hardfork_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_512, uint32_t hardfork = HIVE_BLOCKCHAIN_VERSION.minor_v() );
  virtual ~hardfork_database_fixture();
};

struct genesis_database_fixture : public clean_database_fixture
{
  genesis_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_512 );
  virtual ~genesis_database_fixture();
};

struct curation_database_fixture : public clean_database_fixture
{
  curation_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_512 );
  virtual ~curation_database_fixture();
};

struct cluster_database_fixture
{
  uint16_t shared_file_size_in_mb;

  using ptr_hardfork_database_fixture = std::unique_ptr<hardfork_database_fixture>;

  using content_method = std::function<void( ptr_hardfork_database_fixture& )>;

  cluster_database_fixture( uint16_t _shared_file_size_in_mb = database_fixture::shared_file_size_in_mb_512 );
  virtual ~cluster_database_fixture();

  template<uint8_t hardfork>
  void execute_hardfork( content_method content )
  {
    ptr_hardfork_database_fixture executor( new hardfork_database_fixture( shared_file_size_in_mb, hardfork ) );
    content( executor );
  }

};

struct live_database_fixture : public database_fixture
{
  live_database_fixture();
  virtual ~live_database_fixture();

  fc::path _chain_dir;
};

#ifdef HIVE_ENABLE_SMT
template< typename T >
struct t_smt_database_fixture : public T
{
  using units = flat_map< account_name_type, uint16_t >;

  using database_fixture::set_price_feed;
  using database_fixture::fund;
  using database_fixture::convert;

  t_smt_database_fixture(){}
  virtual ~t_smt_database_fixture(){}

  asset_symbol_type create_smt( const string& account_name, const fc::ecc::private_key& key,
    uint8_t token_decimal_places );
  asset_symbol_type create_smt_with_nai( const string& account_name, const fc::ecc::private_key& key,
    uint32_t nai, uint8_t token_decimal_places );

  /// Creates 3 different SMTs for provided control account, one with 0 precision, the other two with the same non-zero precision.
  std::array<asset_symbol_type, 3> create_smt_3(const char* control_account_name, const fc::ecc::private_key& key);
  /// Tries to create SMTs with too big precision or invalid name.
  void create_invalid_smt( const char* control_account_name, const fc::ecc::private_key& key );

  void push_invalid_operation( const operation& invalid_op, const fc::ecc::private_key& key );
  /// Tries to create SMTs matching existing one. First attempt with matching precision, second one with different (but valid) precision.
  void create_conflicting_smt( const asset_symbol_type existing_smt, const char* control_account_name, const fc::ecc::private_key& key );

  //smt_setup_operation
  smt_generation_unit get_generation_unit ( const units& hive_unit = units(), const units& token_unit = units() );
  smt_capped_generation_policy get_capped_generation_policy
  (
    const smt_generation_unit& pre_soft_cap_unit = smt_generation_unit(),
    const smt_generation_unit& post_soft_cap_unit = smt_generation_unit(),
    uint16_t soft_cap_percent = 0,
    uint32_t min_unit_ratio = 0,
    uint32_t max_unit_ratio = 0
  );
};

using smt_database_fixture = t_smt_database_fixture< clean_database_fixture >;
using smt_database_fixture_for_plugin = t_smt_database_fixture< database_fixture >;

#endif

struct dhf_database
{
  struct create_proposal_data
  {
    std::string creator    ;
    std::string receiver   ;
    fc::time_point_sec start_date ;
    fc::time_point_sec end_date   ;
    hive::protocol::asset daily_pay ;
    std::string subject ;
    std::string url     ;

    create_proposal_data(fc::time_point_sec _start)
    {
      creator    = "alice";
      receiver   = "bob";
      start_date = _start     + fc::days( 1 );
      end_date   = start_date + fc::days( 2 );
      daily_pay  = asset( 100, HBD_SYMBOL );
      subject    = "hello";
      url        = "http:://something.html";
    }
  };
};

struct dhf_database_fixture_performance : public clean_database_fixture
{
  dhf_database_fixture_performance( uint16_t shared_file_size_in_mb = 512 )
                  : clean_database_fixture( shared_file_size_in_mb )
  {
    db->get_benchmark_dumper().set_enabled( true );
    db_plugin->debug_update( []( database& db )
    {
      db.set_remove_threshold( -1 );
    });
  }
};


struct hf23_database_fixture : public clean_database_fixture
{
    hf23_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 )
                    : clean_database_fixture( shared_file_size_in_mb ){}
    virtual ~hf23_database_fixture(){}
};

struct hf24_database_fixture : public clean_database_fixture
{
  hf24_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 )
    : clean_database_fixture( shared_file_size_in_mb )
  {}
  virtual ~hf24_database_fixture() {}
};

struct delayed_vote_database_fixture : public clean_database_fixture
{
  public:

    delayed_vote_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 );
    virtual ~delayed_vote_database_fixture();

    void witness_vote( const std::string& account, const std::string& witness, const bool approve, const fc::ecc::private_key& key );

    share_type get_votes( const string& witness_name );
    int32_t get_user_voted_witness_count( const account_name_type& name );

    asset to_vest( const asset& liquid, const bool to_reward_balance = false );
    time_point_sec move_forward_with_update( const fc::microseconds& time, delayed_voting::opt_votes_update_data_items& items );

    template< typename COLLECTION >
    fc::optional< size_t > get_position_in_delayed_voting_array( const COLLECTION& collection, size_t day, size_t minutes );

    template< typename COLLECTION >
    bool check_collection( const COLLECTION& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val );

    template< typename COLLECTION >
    bool check_collection( const COLLECTION& collection, const bool withdraw_executor, const share_type val, const account_object& obj );
};

struct json_rpc_database_fixture : public database_fixture
{
  private:
    hive::plugins::json_rpc::json_rpc_plugin* rpc_plugin;

    fc::variant get_answer( std::string& request );
    void review_answer( fc::variant& answer, int64_t code, bool is_warning, bool is_fail, fc::optional< fc::variant > id,
      const char* message = nullptr );

  public:

    json_rpc_database_fixture();
    virtual ~json_rpc_database_fixture();

    void make_array_request( std::string& request, int64_t code = 0, bool is_warning = false, bool is_fail = true );
    fc::variant make_request( std::string& request, int64_t code = 0, bool is_warning = false, bool is_fail = true,
      const char* message = nullptr );
    void make_positive_request( std::string& request );
};

namespace test
{
  std::shared_ptr<full_block_type> _generate_block( hive::plugins::chain::abstract_block_producer& bp, const fc::time_point_sec _block_ts, const hive::protocol::account_name_type& _wo,
    const fc::ecc::private_key& _key, uint32_t _skip = 0 );
  bool _push_block( database& db, const block_header& header, const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions, const fc::ecc::private_key& signer, uint32_t skip_flags = 0 );
  bool _push_block( database& db, const std::shared_ptr<full_block_type>& b, uint32_t skip_flags = 0 );
  void _push_transaction( database& db, const signed_transaction& tx, const fc::ecc::private_key& key = fc::ecc::private_key(), uint32_t skip_flags = 0,
    hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack(), fc::ecc::canonical_signature_type _sig_type = fc::ecc::fc_canonical );
}

namespace performance
{
  struct initial_data
  {
    std::string account;

    fc::ecc::private_key key;

    initial_data( database_fixture* db, const std::string& _account );
  };

  std::vector< initial_data > generate_accounts( database_fixture* db, int32_t number_accounts );
}

} }
