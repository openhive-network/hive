#pragma once

#include "hived_fixture.hpp"

#include <fc/log/logger_config.hpp>

namespace hive { namespace chain {

struct clean_database_fixture : public hived_fixture
{
  clean_database_fixture( 
    uint16_t shared_file_size_in_mb = shared_file_size_big,
    fc::optional<uint32_t> hardfork = fc::optional<uint32_t>(),
    bool init_ah_plugin = true, int block_log_split = 9999 );
  virtual ~clean_database_fixture();

  void validate_database();
  void inject_hardfork( uint32_t hardfork );
};

struct pruned_database_fixture : public clean_database_fixture
{
  pruned_database_fixture(
    uint16_t shared_file_size_in_mb = shared_file_size_big,
    fc::optional<uint32_t> hardfork = fc::optional<uint32_t>(),
    bool init_ah_plugin = true );
  virtual ~pruned_database_fixture();
};

struct hardfork_database_fixture : public clean_database_fixture
{
  hardfork_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_big, uint32_t hardfork = HIVE_BLOCKCHAIN_VERSION.minor_v() );
  virtual ~hardfork_database_fixture();
};

struct cluster_database_fixture
{
  uint16_t shared_file_size_in_mb;

  using ptr_hardfork_database_fixture = std::unique_ptr<hardfork_database_fixture>;

  using content_method = std::function<void( ptr_hardfork_database_fixture& )>;

  cluster_database_fixture( uint16_t _shared_file_size_in_mb = database_fixture::shared_file_size_big );
  virtual ~cluster_database_fixture();

  template<uint8_t hardfork>
  void execute_hardfork( content_method content )
  {
    ptr_hardfork_database_fixture executor( new hardfork_database_fixture( shared_file_size_in_mb, hardfork ) );
    content( executor );
  }

};

struct config_database_fixture : public clean_database_fixture
{
  config_database_fixture( std::function< void() > action, uint16_t shared_file_size_in_mb = shared_file_size_big );
  virtual ~config_database_fixture();
};

struct genesis_database_fixture : public clean_database_fixture
{
  genesis_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_big );
  virtual ~genesis_database_fixture();
};

struct curation_database_fixture : public config_database_fixture
{
  curation_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_big );
  virtual ~curation_database_fixture();
};

#ifdef HIVE_ENABLE_SMT
template< typename T >
struct t_smt_database_fixture : public T
{
  using units = flat_map< account_name_type, uint16_t >;

  using database_fixture::set_price_feed;
  using database_fixture::fund;

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
using smt_database_fixture_for_plugin = t_smt_database_fixture< hived_fixture >;

#endif

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
    hf23_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_small )
                    : clean_database_fixture( shared_file_size_in_mb ){}
    virtual ~hf23_database_fixture(){}
};

struct hf24_database_fixture : public clean_database_fixture
{
  hf24_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_small )
    : clean_database_fixture( shared_file_size_in_mb )
  {}
  virtual ~hf24_database_fixture() {}
};

struct delayed_vote_database_fixture : public config_database_fixture
{
  public:

    delayed_vote_database_fixture( uint16_t shared_file_size_in_mb = shared_file_size_small );
    virtual ~delayed_vote_database_fixture();

    void witness_vote( const std::string& account, const std::string& witness, const bool approve, const fc::ecc::private_key& key );

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

} }
