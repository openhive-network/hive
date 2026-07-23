#pragma once

#include <appbase/application.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/util/balance.hpp>
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
#include <hive/plugins/debug_node/debug_node_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>

#include <array>
#include <iostream>

#define HIVE_INITIAL_TEST_SUPPLY HIVE_asset( 500'000'000'000ll )
#define HBD_INITIAL_TEST_SUPPLY  HBD_asset( 10'000'000'000ll )
#define HP_INITIAL_TEST_SUPPLY   HIVE_asset( 200'000'000'000ll )

extern uint32_t HIVE_TESTING_GENESIS_TIMESTAMP;

#define PUSH_TX \
  hive::chain::test::_push_transaction

#define GENERATE_BLOCK \
  hive::chain::test::_generate_block

#define HIVE_REQUIRE_THROW( expr, exc_type )           \
  BOOST_REQUIRE_THROW( expr, exc_type )

#define HIVE_CHECK_THROW( expr, exc_type )             \
do {                                                   \
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
} while(false)

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

// Default amount of VESTS (expressed in HIVE) implicitly granted to accounts created via ACTOR(S)
// macros. Most tests are not sensitive to the exact value; tests that are should pass an explicit
// amount (or NO_VESTING) and verify the chosen value does not change their results.
#define DEFAULT_VESTING HIVE_asset( 100 )
// Use when the created account should receive no initial vesting at all.
#define NO_VESTING HIVE_asset( 0 )

#define PREP_ACTOR(name) \
  fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name));   \
  fc::ecc::private_key name ## _post_key = generate_private_key(std::string( BOOST_PP_STRINGIZE(name) ) + "_post" ); \
  public_key_type name ## _public_key = name ## _private_key.get_public_key()

#define ACTOR(initial_vesting, name) \
  PREP_ACTOR(name); \
  account_create(BOOST_PP_STRINGIZE(name), name ## _public_key, name ## _post_key.get_public_key(), initial_vesting); \
  account_id_type name ## _id = get_account_id(BOOST_PP_STRINGIZE(name)); (void)name ## _id

#define ACTORS_IMPL(r, data, elem) ACTOR(data, elem);
#define ACTORS(initial_vesting, names) BOOST_PP_SEQ_FOR_EACH(ACTORS_IMPL, initial_vesting, names) \
  validate_database()

#define PREP_ACTOR_EXT(object, name) \
  fc::ecc::private_key name ## _private_key = object.generate_private_key(BOOST_PP_STRINGIZE(name));   \
  fc::ecc::private_key name ## _post_key = object.generate_private_key(std::string( BOOST_PP_STRINGIZE(name) ) + "_post" ); \
  public_key_type name ## _public_key = name ## _private_key.get_public_key()

#define ACTOR_EXT(object, initial_vesting, name) \
  PREP_ACTOR_EXT(object, name); \
  object.account_create(BOOST_PP_STRINGIZE(name), name ## _public_key, name ## _post_key.get_public_key(), initial_vesting); \
  account_id_type name ## _id = object.get_account_id(BOOST_PP_STRINGIZE(name)); (void)name ## _id

#define ACTORS_EXT_IMPL(r, data, elem) ACTOR_EXT(BOOST_PP_TUPLE_ELEM(2, 0, data), BOOST_PP_TUPLE_ELEM(2, 1, data), elem);
#define ACTORS_EXT(object, initial_vesting, names) BOOST_PP_SEQ_FOR_EACH(ACTORS_EXT_IMPL, (object, initial_vesting), names) \
  object.validate_database()

#define ASSET( s ) \
  hive::protocol::legacy_asset::from_string( s ).to_asset()

#define ISSUE_FUNDS( account_name, _asset ) \
  issue_funds( account_name, _asset ); \
  generate_block()

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
fc::path common_init( appbase::application& app, bool remove_db_files, const fc::path& data_dir,
  const std::function< void( appbase::application& app, int argc, char** argv ) >& app_initializer );

struct empty_fixture {};

struct database_fixture {
  // the reason we use an app is to exercise the indexes of built-in
  //   plugins
  chain::database* db = nullptr;
  fc::ecc::private_key private_key = fc::ecc::private_key::generate();
  fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
  public_key_type init_account_pub_key = init_account_priv_key.get_public_key();
  uint32_t default_skip = database::skip_nothing;

  typedef plugins::account_history_rocksdb::account_history_rocksdb_plugin ah_plugin_type;
  ah_plugin_type* ah_plugin = nullptr;
  plugins::witness::witness_plugin* witness_plugin = nullptr;
  plugins::debug_node::debug_node_plugin* db_plugin = nullptr;

  optional<fc::temp_directory> data_dir;

  appbase::application theApp;
  hive::chain::blockchain_worker_thread_pool* thread_pool = nullptr;

  database_fixture() {}
  virtual ~database_fixture() {}

  virtual hive::plugins::chain::chain_plugin& get_chain_plugin() const = 0;

  static fc::ecc::private_key generate_private_key( string seed = "init_key" );

  static const fc::ecc::private_key committee;
  static const uint16_t shared_file_size_small = 64;
  static const uint16_t shared_file_size_big = 512;

  void generate_block(uint32_t skip = 0,
                      const fc::ecc::private_key& key = committee,
                      int miss_blocks = 0);

  /**
    * Generates block "in queen mode" - putting pending transactions into block without checking.
    * Best used with db_plugin->debug_push_pending_transaction
    */
  void generate_block_from_pending();

  uint32_t generate_blocks( const fc::ecc::private_key& key, uint32_t count, uint32_t skip );

  uint32_t generate_blocks_until( const fc::ecc::private_key& key, const fc::time_point_sec& head_block_time,
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

  void account_create(
    const string& name,
    const string& creator,
    const private_key_type& creator_key,
    const HIVE_asset& initial_vesting,
    const public_key_type& key,
    const public_key_type& post_key,
    const string& json_metadata
  );

  void account_create(
    const string& name,
    const public_key_type& key,
    const public_key_type& post_key,
    const HIVE_asset& initial_vesting
  );

  void account_create(
    const string& name,
    const public_key_type& key
  );

  void witness_create(
    const string& owner,
    const private_key_type& owner_key,
    const string& url,
    const public_key_type& signing_key,
    const HIVE_asset& fee
  );

  void account_update( const string& account, const fc::ecc::public_key& memo_key, const string& metadata,
                       optional<authority> owner, optional<authority> active, optional<authority> posting,
                       const fc::ecc::private_key& key );
  void account_update2( const string& account, optional<authority> owner, optional<authority> active, optional<authority> posting,
                        optional<public_key_type> memo_key, const string& metadata, const string& posting_metadata,
                        const fc::ecc::private_key& key );

  full_transaction_ptr build_transaction( const operation& op, const fc::ecc::private_key& key );
  full_transaction_ptr build_transaction( const signed_transaction& tx, const std::vector<fc::ecc::private_key>& keys,
    hive::protocol::pack_type pack = hive::protocol::serialization_mode_controller::get_current_pack() );

  full_transaction_ptr push_transaction( const operation& op, const fc::ecc::private_key& key );
  full_transaction_ptr push_transaction( const signed_transaction& tx, const fc::ecc::private_key& key = fc::ecc::private_key() ) { return push_transaction_ex( tx, key ); }
  full_transaction_ptr push_transaction( const signed_transaction& tx, const std::vector<fc::ecc::private_key>& keys ) { return push_transaction_ex( tx, keys ); }
  full_transaction_ptr push_transaction_ex( const signed_transaction& tx, const fc::ecc::private_key& key = fc::ecc::private_key(),
    uint32_t skip_flags = 0, hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack() );
  full_transaction_ptr push_transaction_ex( const signed_transaction& tx, const std::vector<fc::ecc::private_key>& keys,
    uint32_t skip_flags = 0, hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack() );

  void fund( const string& account_name, const HIVE_asset& amount ); //transfer from initminer
  void fund( const string& account_name, const HBD_asset& amount ); //transfer from initminer
  void issue_funds( const string& account_name, const HIVE_asset& amount, bool update_print_rate = true );
  void issue_funds( const string& account_name, const HBD_asset& amount, bool update_print_rate = true );
  void transfer( const string& from, const string& to, const asset& amount, const std::string& memo, const fc::ecc::private_key& key );
  void recurrent_transfer( const string& from, const string& to, const asset& amount, const string& memo, uint16_t recurrence,
                           uint16_t executions, const fc::ecc::private_key& key );
  void convert_hbd_to_hive( const std::string& owner, uint32_t requestid, const HBD_asset& amount, const fc::ecc::private_key& key );
  void collateralized_convert_hive_to_hbd( const std::string& owner, uint32_t requestid, const HIVE_asset& amount, const fc::ecc::private_key& key );
  void vest( const string& to, const HIVE_asset& amount ); //vesting from initminer
  void vest( const string& from, const string& to, const HIVE_asset& amount, const fc::ecc::private_key& key );
  void delegate_vest( const string& delegator, const string& delegatee, const VEST_asset& amount, const fc::ecc::private_key& key );
  void set_withdraw_vesting_route(const string& from, const string& to, uint16_t percent, bool auto_vest, const fc::ecc::private_key& key);
  void withdraw_vesting( const string& account, const VEST_asset& amount, const fc::ecc::private_key& key );
  void proxy( const string& _account, const string& _proxy, const fc::ecc::private_key& _key );
  void set_price_feed( const HBD_price& new_price, bool stop_at_update_block = false ); //sets on initminer(s)
  void set_witness_props( const flat_map< string, vector< char > >& new_props, bool wait_for_activation = true ); //sets on initminer(s)
  //forces the account creation fee on the schedule median and on every witness (so it survives median recomputation)
  void set_account_creation_fee( const HIVE_asset& fee );
  void witness_feed_publish( const string& publisher, const HBD_price& exchange_rate, const private_key_type& key );
  share_type get_votes( const string& witness_name );
  void witness_vote( account_name_type voter, account_name_type witness, const fc::ecc::private_key& key, bool approve = true );
  void limit_order_create( const string& owner, const asset& amount_to_sell, const asset& min_to_receive, bool fill_or_kill,
                           const fc::microseconds& expiration_shift, uint32_t orderid, const fc::ecc::private_key& key);
  void limit_order_cancel( const string& owner, uint32_t orderid, const fc::ecc::private_key& key );
  void limit_order2_create( const string& owner, const asset& amount_to_sell, const price& exchange_rate, bool fill_or_kill,
                            const fc::microseconds& expiration_shift, uint32_t orderid, const fc::ecc::private_key& key );
  void escrow_transfer( const string& from, const string& to, const string& agent, const HIVE_asset& hive_amount,
                        const HBD_asset& hbd_amount, const asset& fee, const std::string& json_meta, const fc::microseconds& ratification_shift,
                        const fc::microseconds& expiration_shift, uint32_t escrow_id, const fc::ecc::private_key& key );
  void escrow_approve( const string& from, const string& to, const string& agent, const string& who, bool approve, uint32_t escrow_id, const fc::ecc::private_key& key );
  void escrow_release( const string& from, const string& to, const string& agent, const string& who, const string& receiver,
                       const HIVE_asset& hive_amount, const HBD_asset& hbd_amount, uint32_t escrow_id, const fc::ecc::private_key& key );
  void escrow_dispute( const string& from, const string& to, const string& agent, const string& who, uint32_t escrow_id, const fc::ecc::private_key& key );
  void transfer_to_savings( const string& from, const string& to, const asset& amount, const string& memo, const fc::ecc::private_key& key );
  void transfer_from_savings( const string& from, const string& to, const asset& amount, const string& memo, uint32_t request_id,
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
    HBD_asset daily_pay ;
    std::string subject ;
    std::string url     ;

    create_proposal_data(fc::time_point_sec _start)
    {
      creator    = "alice";
      receiver   = "bob";
      start_date = _start     + fc::days( 1 );
      end_date   = start_date + fc::days( 2 );
      daily_pay  = HBD_asset( 100 );
      subject    = "hello";
      url        = "http:://something.html";
    }
  };

  int64_t create_proposal( const std::string& creator, const std::string& receiver, const std::string& subject, const std::string& permlink,
                           const time_point_sec& start_date, const time_point_sec& end_date, const HBD_asset& daily_pay, const fc::ecc::private_key& active_key );
  int64_t create_proposal( const std::string& creator, const std::string& receiver,
                    const time_point_sec& start_date, const time_point_sec& end_date,
                    const HBD_asset& daily_pay, const fc::ecc::private_key& active_key, const fc::ecc::private_key& post_key, bool with_block_generation = true );
  void vote_proposal( const std::string& voter, const std::vector< int64_t >& id_proposals, bool approve, const fc::ecc::private_key& key );
  void remove_proposal(account_name_type _deleter, flat_set<int64_t> _proposal_id, const fc::ecc::private_key& _key);
  void update_proposal( uint64_t proposal_id, const std::string& creator, const HBD_asset& daily_pay, const std::string& subject, const std::string& permlink, const fc::ecc::private_key& key, time_point_sec* end_date = nullptr );

  bool exist_proposal( int64_t id );
  const proposal_object* find_proposal( int64_t id );
  const proposal_vote_object* find_vote_for_proposal(const std::string& _user, int64_t _proposal_id);
  uint64_t get_nr_blocks_until_proposal_maintenance_block();
  uint64_t get_nr_blocks_until_daily_proposal_maintenance_block();

  account_id_type get_account_id( const string& account_name )const;
  HIVE_asset get_hive_balance( const string& account_name )const;
  HBD_asset get_hbd_balance( const string& account_name )const;
  HIVE_asset get_hive_savings( const string& account_name )const;
  HBD_asset get_hbd_savings( const string& account_name )const;
  HIVE_asset get_hive_rewards( const string& account_name )const;
  HBD_asset get_hbd_rewards( const string& account_name )const;
  VEST_asset get_vesting( const string& account_name )const;
  VEST_asset get_vest_rewards( const string& account_name )const;
  HIVE_asset get_vest_rewards_as_hive( const string& account_name )const;

  comment get_comment( const string& author, const string& permlink )const;

private:

  void post_comment_internal( const std::string& _author, const std::string& _permlink, const std::string& _title, const std::string& _body,
                              const std::string& _parent_author, const std::string& _parent_permlink, const fc::ecc::private_key& _key );

public:

  void post_comment_with_block_generation( const std::string& _author, const std::string& _permlink, const std::string& _title, const std::string& _body, const std::string& _category, const fc::ecc::private_key& _post_key );
  void post_comment( const std::string& _author, const std::string& _permlink, const std::string& _title, const std::string& _body, const std::string& _category, const fc::ecc::private_key& _post_key );
  void post_comment_to_comment( const std::string& author, const std::string& permlink, const std::string& title, const std::string& body,
                                const std::string& parent_author, const std::string& parent_permlink, const fc::ecc::private_key& post_key );
  void delete_comment( const std::string& _author, const std::string& _permlink, const fc::ecc::private_key& _key );
  void set_comment_options( const std::string& author, const std::string& permlink, const HBD_asset& max_accepted_payout, uint16_t percent_hbd,
                            bool allow_curation_rewards, bool allow_votes, const fc::ecc::private_key& post_key );
  void set_comment_options( const std::string& author, const std::string& permlink, const HBD_asset& max_accepted_payout, uint16_t percent_hbd,
                            bool allow_curation_rewards, bool allow_votes, const comment_options_extensions_type& extensions, const fc::ecc::private_key& post_key );
  void vote( const std::string& _author, const std::string& _permlink, const std::string& _voter, int16_t _weight, const fc::ecc::private_key& _key );
  void claim_reward_balance( const std::string& account, const HIVE_asset& reward_hive, const HBD_asset& reward_hbd, const VEST_asset& reward_vests,
                             const fc::ecc::private_key& key );
  /// @brief Creates proof of work and account for the worker.
  /// @param _name Name of the worker (and account to be created too).
  /// @param _public_key worker (account) public key
  /// @param _private_key worker (account) private key
  void create_with_pow( const std::string& _name, const fc::ecc::public_key& _public_key, const fc::ecc::private_key& _private_key );
  /// Same as create_with_pow but uses pow2 operation.
  void create_with_pow2( const std::string& _name, const fc::ecc::public_key& _public_key, const fc::ecc::private_key& _private_key );
  void create_with_delegation( const std::string& creator, const std::string& new_account_name, const fc::ecc::public_key& public_key,
                               const fc::ecc::private_key& posting_key, const VEST_asset& delegation, const fc::ecc::private_key& key );
  void claim_account( const std::string& creator, const HIVE_asset& fee, const fc::ecc::private_key& key );
  void create_claimed_account( const std::string& creator, const std::string& new_account_name, const fc::ecc::public_key& public_key,
                               const fc::ecc::public_key& posting_key, const string& json_metadata, const fc::ecc::private_key& key );
  void change_recovery_account( const std::string& account_to_recover, const std::string& new_recovery_account, const fc::ecc::private_key& key );
  void request_account_recovery( const std::string& recovery_account, const std::string& account_to_recover, 
                                 const authority& new_owner_authority, const fc::ecc::private_key& key );
  void recover_account( const std::string& account_to_recover, const fc::ecc::private_key& new_owner_key, 
                        const fc::ecc::private_key& recent_owner_key );

  bool compare_delayed_vote_count( const account_name_type& name, const std::vector<uint64_t>& data_to_compare );

  vector< operation > get_last_operations( uint32_t ops );

  static std::string asset_to_string( const asset& a );
  template< uint32_t _SYMBOL >
  static std::string asset_to_string( const tiny_asset<_SYMBOL>& a ) { return asset_to_string( a.to_asset() ); }

  void validate_database();
};

struct dhf_database
{
  struct create_proposal_data
  {
    std::string creator    ;
    std::string receiver   ;
    fc::time_point_sec start_date ;
    fc::time_point_sec end_date   ;
    HBD_asset daily_pay ;
    std::string subject ;
    std::string url     ;

    create_proposal_data(fc::time_point_sec _start)
    {
      creator    = "alice";
      receiver   = "bob";
      start_date = _start     + fc::days( 1 );
      end_date   = start_date + fc::days( 2 );
      daily_pay  = HBD_asset( 100 );
      subject    = "hello";
      url        = "http:://something.html";
    }
  };
};

namespace test
{
  std::shared_ptr<full_block_type> _generate_block( hive::plugins::chain::abstract_block_producer& bp, const fc::time_point_sec _block_ts, const hive::protocol::account_name_type& _wo,
    const fc::ecc::private_key& _key, uint32_t _skip = 0 );
  void _push_transaction( hive::plugins::chain::chain_plugin& cp, const signed_transaction& tx,
    const fc::ecc::private_key& key = fc::ecc::private_key(), uint32_t skip_flags = 0,
    hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack() );
}

namespace performance
{
  struct initial_data
  {
    std::string account;

    fc::ecc::private_key active_key;
    fc::ecc::private_key post_key;

    initial_data( database_fixture* db, const std::string& _account, const HIVE_asset& initial_vesting = DEFAULT_VESTING );
  };

  std::vector< initial_data > generate_accounts( database_fixture* db, int32_t number_accounts, const HIVE_asset& initial_vesting = DEFAULT_VESTING );
}

} }

namespace hive { namespace protocol {

inline std::ostream& operator<<( std::ostream& os, const asset& a )
{
  return os << hive::chain::database_fixture::asset_to_string( a );
}

template< uint32_t _SYMBOL >
inline std::ostream& operator<<( std::ostream& os, const tiny_asset<_SYMBOL>& a )
{
  return os << hive::chain::database_fixture::asset_to_string( a.to_asset() );
}

inline std::ostream& operator<<( std::ostream& os, const price& p )
{
  return os << "price( " << p.get_base() << " / " << p.get_quote() << " )";
}

template< uint32_t _BASE, uint32_t _QUOTE >
inline std::ostream& operator<<( std::ostream& os, const tiny_price<_BASE,_QUOTE>& p )
{
  return os << p.to_price();
}

template< typename Storage >
inline std::ostream& operator<<( std::ostream& os, const fixed_string_impl<Storage>& s )
{
  return os << std::string( s );
}

} } // hive::protocol

namespace hive { namespace chain {

inline std::ostream& operator<<( std::ostream& os, const balance& b )
{
  return os << hive::chain::database_fixture::asset_to_string( b.as_asset() );
}

inline std::ostream& operator<<( std::ostream& os, const temp_balance& b )
{
  return os << hive::chain::database_fixture::asset_to_string( b.as_asset() );
}

template< uint32_t _SYMBOL >
inline std::ostream& operator<<( std::ostream& os, const tiny_balance<_SYMBOL>& b )
{
  return os << hive::chain::database_fixture::asset_to_string( b.as_asset().to_asset() );
}

template< uint32_t _SYMBOL >
inline std::ostream& operator<<( std::ostream& os, const temp_tiny_balance<_SYMBOL>& b )
{
  return os << hive::chain::database_fixture::asset_to_string( b.as_asset().to_asset() );
}

} } // hive::chain

namespace fc
{

inline std::ostream& operator<<( std::ostream& ostr, const time_point_sec time )
{
  ostr << time.to_iso_string();
  return ostr;
}

template <typename T>
inline std::ostream& operator<<( std::ostream& ostr, const safe<T> v )
{
  ostr << v.value;
  return ostr;
}

}
