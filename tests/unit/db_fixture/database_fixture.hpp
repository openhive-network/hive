#pragma once

#include <appbase/application.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <hive/plugins/block_api/block_api_plugin.hpp>
#include <hive/protocol/asset.hpp>
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
  db.push_transaction( trx, ~0 ); \
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
  HIVE_REQUIRE_THROW(db.push_transaction(trx, ~0), exc_type); \
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

#define OP2TX(OP,TX,KEY) \
TX.operations.push_back( OP ); \
TX.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION ); \
TX.sign( KEY, db->get_chain_id(), fc::ecc::bip_0062 );

#define PUSH_OP(OP,KEY) \
{ \
  signed_transaction tx; \
  OP2TX(OP,tx,KEY) \
  db->push_transaction( tx, 0 ); \
}

#define PUSH_OP_TWICE(OP,KEY) \
{ \
  signed_transaction tx; \
  OP2TX(OP,tx,KEY) \
  db->push_transaction( tx, 0 ); \
  db->push_transaction( tx, database::skip_transaction_dupe_check ); \
}

#define FAIL_WITH_OP(OP,KEY,EXCEPTION) \
{ \
  signed_transaction tx; \
  OP2TX(OP,tx,KEY) \
  HIVE_REQUIRE_THROW( db->push_transaction( tx, 0 ), EXCEPTION ); \
}

namespace hive { namespace chain {

using namespace hive::protocol;

struct database_fixture {
  // the reason we use an app is to exercise the indexes of built-in
  //   plugins
  chain::database* db = nullptr;
  signed_transaction trx;
  public_key_type committee_key;
  account_id_type committee_account;
  fc::ecc::private_key private_key = fc::ecc::private_key::generate();
  fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
  string debug_key = hive::utilities::key_to_wif( init_account_priv_key );
  public_key_type init_account_pub_key = init_account_priv_key.get_public_key();
  uint32_t default_skip = 0 | database::skip_undo_history_check | database::skip_authority_check;
  fc::ecc::canonical_signature_type default_sig_canon = fc::ecc::fc_canonical;

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

  void open_database( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 );
  void generate_block(uint32_t skip = 0,
                      const fc::ecc::private_key& key = generate_private_key("init_key"),
                      int miss_blocks = 0);

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

  void push_transaction( const operation& op, const fc::ecc::private_key& key );

  void fund( const string& account_name, const share_type& amount = 500000 );
  void fund( const string& account_name, const asset& amount, bool update_print_rate = true );
  void transfer( const string& from, const string& to, const asset& amount );
  void convert( const string& account_name, const asset& amount );
  void vest( const string& from, const string& to, const asset& amount );
  void vest( const string& from, const share_type& amount );
  void vest( const string& from, const string& to, const asset& amount, const fc::ecc::private_key& key );
  void proxy( const string& account, const string& proxy );
  void set_price_feed( const price& new_price, bool stop_at_update_block = false );
  void set_witness_props( const flat_map< string, vector< char > >& new_props );
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

  void post_comment_internal( const std::string& _author, const std::string& _permlink, const std::string& _title, const std::string& _body, const std::string& _parent_permlink, const fc::ecc::private_key& _key );

public:

  void post_comment_with_block_generation( std::string _author, std::string _permlink, std::string _title, std::string _body, std::string _parent_permlink, const fc::ecc::private_key& _key );
  void post_comment( std::string _author, std::string _permlink, std::string _title, std::string _body, std::string _parent_permlink, const fc::ecc::private_key& _key);
  void vote( std::string _author, std::string _permlink, std::string _voter, int16_t _weight, const fc::ecc::private_key& _key );

  void sign( signed_transaction& trx, const fc::ecc::private_key& key );

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

struct sps_proposal_database_fixture : public virtual clean_database_fixture
{
  sps_proposal_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 )
                  : clean_database_fixture( shared_file_size_in_mb ){}
  virtual ~sps_proposal_database_fixture(){}

  void plugin_prepare();

  int64_t create_proposal(   std::string creator, std::string receiver,
                    time_point_sec start_date, time_point_sec end_date,
                    asset daily_pay, const fc::ecc::private_key& key, bool with_block_generation = true );

  void vote_proposal( std::string voter, const std::vector< int64_t >& id_proposals, bool approve, const fc::ecc::private_key& key );

  bool exist_proposal( int64_t id );
  const proposal_object* find_proposal( int64_t id );

  void remove_proposal(account_name_type _deleter, flat_set<int64_t> _proposal_id, const fc::ecc::private_key& _key);

  void update_proposal(uint64_t proposal_id, std::string creator, asset daily_pay, std::string subject, std::string permlink, const fc::ecc::private_key& key, time_point_sec* end_date = nullptr );
  bool find_vote_for_proposal(const std::string& _user, int64_t _proposal_id);

  uint64_t get_nr_blocks_until_maintenance_block();
  uint64_t get_nr_blocks_until_daily_maintenance_block();

  void witness_vote( account_name_type _voter, account_name_type _witness, const fc::ecc::private_key& _key, bool _approve = true );
  void proxy( account_name_type _account, account_name_type _proxy, const fc::ecc::private_key& _key );

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

struct sps_proposal_database_fixture_performance : public sps_proposal_database_fixture
{
  sps_proposal_database_fixture_performance( uint16_t shared_file_size_in_mb = 512 )
                  : sps_proposal_database_fixture( shared_file_size_in_mb )
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

    void delegate_vest( const string& delegator, const string& delegatee, const asset& amount, const fc::ecc::private_key& key );
};

struct hf24_database_fixture : public clean_database_fixture
{
  hf24_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_in_mb_64 )
    : clean_database_fixture( shared_file_size_in_mb )
  {}
  virtual ~hf24_database_fixture() {}
};

struct delayed_vote_database_fixture : public virtual clean_database_fixture
{
  public:

    delayed_vote_database_fixture( uint16_t shared_file_size_in_mb = 8 )
                    : clean_database_fixture( shared_file_size_in_mb ){}
    virtual ~delayed_vote_database_fixture(){}

    void witness_vote( const std::string& account, const std::string& witness, const bool approve, const fc::ecc::private_key& key );
    void withdraw_vesting( const string& account, const asset& amount, const fc::ecc::private_key& key );
    void proxy( const string& account, const string& proxy, const fc::ecc::private_key& key );
    void decline_voting_rights( const string& account, const bool decline, const fc::ecc::private_key& key );

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

struct delayed_vote_proposal_database_fixture 
  :  public delayed_vote_database_fixture,
    public sps_proposal_database_fixture
{
  delayed_vote_proposal_database_fixture( uint16_t shared_file_size_in_mb = 8 )
                    :  delayed_vote_database_fixture( shared_file_size_in_mb ),
                      sps_proposal_database_fixture( shared_file_size_in_mb ) {}
  virtual ~delayed_vote_proposal_database_fixture(){}
};

struct json_rpc_database_fixture : public database_fixture
{
  private:
    hive::plugins::json_rpc::json_rpc_plugin* rpc_plugin;

    fc::variant get_answer( std::string& request );
    void review_answer( fc::variant& answer, int64_t code, bool is_warning, bool is_fail, fc::optional< fc::variant > id );

  public:

    json_rpc_database_fixture();
    virtual ~json_rpc_database_fixture();

    void make_array_request( std::string& request, int64_t code = 0, bool is_warning = false, bool is_fail = true );
    fc::variant make_request( std::string& request, int64_t code = 0, bool is_warning = false, bool is_fail = true );
    void make_positive_request( std::string& request );
};

namespace test
{
  bool _push_block( database& db, const signed_block& b, uint32_t skip_flags = 0 );
  void _push_transaction( database& db, const signed_transaction& tx, uint32_t skip_flags = 0 );
}

} }
