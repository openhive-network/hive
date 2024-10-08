#include "clean_database_fixture.hpp"

#include <hive/manifest/plugins.hpp>

#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/utilities/database_configuration.hpp>
#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <boost/test/unit_test.hpp>

namespace bpo = boost::program_options;

namespace hive { namespace chain {


clean_database_fixture::clean_database_fixture( 
  uint16_t shared_file_size_in_mb, fc::optional<uint32_t> hardfork, bool init_ah_plugin, int block_log_split )
{
  try {

  configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );
  configuration_data.allow_not_enough_rc = true;

  if( init_ah_plugin )
  {
    postponed_init(
      {
        config_line_t( { "plugin",
          { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME,
            HIVE_WITNESS_PLUGIN_NAME } }
        ),
        config_line_t( { "shared-file-size",
          { std::to_string( 1024 * 1024 * shared_file_size_in_mb ) } }
        ),
        config_line_t( { "block-log-split",
          { std::to_string( block_log_split ) } }
        )
      },
      &ah_plugin
    );
  }
  else
  {
    postponed_init(
      {
        config_line_t( { "plugin",
          { HIVE_WITNESS_PLUGIN_NAME } }
        ),
        config_line_t( { "shared-file-size",
          { std::to_string( 1024 * 1024 * shared_file_size_in_mb ) } }
        ),
        config_line_t( { "block-log-split",
          { std::to_string( block_log_split ) } }
        )
      }
    );
  }

  init_account_pub_key = init_account_priv_key.get_public_key();

  inject_hardfork( hardfork.valid() ? ( *hardfork ) : HIVE_BLOCKCHAIN_VERSION.minor_v() );
  db->_log_hardforks = true;

  vest( HIVE_INIT_MINER_NAME, ASSET( "10.000 TESTS" ) );

  // Fill up the rest of the required miners
  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
    fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
    witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
  }

  validate_database();
  } catch ( const fc::exception& e )
  {
    edump( (e.to_detail_string()) );
    throw;
  }

  return;
}

clean_database_fixture::~clean_database_fixture() {}

void clean_database_fixture::validate_database()
{
  database_fixture::validate_database();

  //validate RC
  if( db->has_hardfork( HIVE_HARDFORK_0_20 ) )
  {
    const auto& idx = db->get_index< account_index, by_name >();
    for( const account_object& account : idx )
    {
      int64_t max_rc = account.get_maximum_rc().value;
      FC_ASSERT( max_rc == account.last_max_rc,
        "Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b} in validate_database()",
        ( "a", account.get_name() )( "old", account.last_max_rc )( "new", max_rc )( "b", db->head_block_num() ) );
    }
  }
}

void clean_database_fixture::inject_hardfork( uint32_t hardfork )
{
  generate_block();
  db->set_hardfork( hardfork );
  generate_block();
}

pruned_database_fixture::pruned_database_fixture( uint16_t shared_file_size_in_mb, fc::optional<uint32_t> hardfork, bool init_ah_plugin )
  : clean_database_fixture( shared_file_size_in_mb, hardfork, init_ah_plugin, 1 )
{
}

pruned_database_fixture::~pruned_database_fixture()
{
}

hardfork_database_fixture::hardfork_database_fixture( uint16_t shared_file_size_in_mb, uint32_t hardfork )
                            : clean_database_fixture( shared_file_size_in_mb, hardfork )
{
}

hardfork_database_fixture::~hardfork_database_fixture()
{
}

cluster_database_fixture::cluster_database_fixture( uint16_t _shared_file_size_in_mb )
                            : shared_file_size_in_mb( _shared_file_size_in_mb )
{
  set_mainnet_cashout_values( false );
}

cluster_database_fixture::~cluster_database_fixture()
{
  configuration_data.reset_cashout_values();
}

config_database_fixture::config_database_fixture( std::function< void() > action, uint16_t shared_file_size_in_mb )
  : clean_database_fixture( ( action(), shared_file_size_in_mb ) )
{}

config_database_fixture::~config_database_fixture()
{}

genesis_database_fixture::genesis_database_fixture( uint16_t shared_file_size_in_mb )
  : clean_database_fixture( shared_file_size_in_mb, 0 )
{}

genesis_database_fixture::~genesis_database_fixture()
{}

curation_database_fixture::curation_database_fixture( uint16_t shared_file_size_in_mb )
  : config_database_fixture( [](){ set_mainnet_cashout_values( false ); }, shared_file_size_in_mb )
{}

curation_database_fixture::~curation_database_fixture()
{
  configuration_data.reset_cashout_values();
}

#ifdef HIVE_ENABLE_SMT

template< typename T >
asset_symbol_type t_smt_database_fixture< T >::create_smt_with_nai( const string& account_name, const fc::ecc::private_key& key,
  uint32_t nai, uint8_t token_decimal_places )
{
  smt_create_operation op;
  signed_transaction tx;
  try
  {
    fund( account_name, ASSET( "5000.000 TESTS" ) );
    fund( account_name, ASSET( "5000.000 TBD" ) );
    this->generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    op.symbol = asset_symbol_type::from_nai( nai, token_decimal_places );
    op.precision = op.symbol.decimals();
    op.smt_creation_fee = this->db->get_dynamic_global_properties().smt_creation_fee;
    op.control_account = account_name;

    tx.operations.push_back( op );
    tx.set_expiration( this->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    this->push_transaction( tx, key, 0, hive::protocol::serialization_mode_controller::get_current_pack() );

    this->generate_block();
  }
  FC_LOG_AND_RETHROW();

  return op.symbol;
}

template< typename T >
asset_symbol_type t_smt_database_fixture< T >::create_smt( const string& account_name, const fc::ecc::private_key& key,
  uint8_t token_decimal_places )
{
  asset_symbol_type symbol;
  try
  {
    auto nai_symbol = this->get_new_smt_symbol( token_decimal_places, this->db );
    symbol = create_smt_with_nai( account_name, key, nai_symbol.to_nai(), token_decimal_places );
  }
  FC_LOG_AND_RETHROW();

  return symbol;
}

void sub_set_create_op( smt_create_operation* op, account_name_type control_acount, chain::database& db )
{
  op->precision = op->symbol.decimals();
  op->smt_creation_fee = db.get_dynamic_global_properties().smt_creation_fee;
  op->control_account = control_acount;
}

void set_create_op( smt_create_operation* op, account_name_type control_account, uint8_t token_decimal_places, chain::database& db )
{
  op->symbol = database_fixture::get_new_smt_symbol( token_decimal_places, &db );
  sub_set_create_op( op, control_account, db );
}

void set_create_op( smt_create_operation* op, account_name_type control_account, uint32_t token_nai, uint8_t token_decimal_places, chain::database& db )
{
  op->symbol.from_nai(token_nai, token_decimal_places);
  sub_set_create_op( op, control_account, db );
}

template< typename T >
std::array<asset_symbol_type, 3> t_smt_database_fixture< T >::create_smt_3(const char* control_account_name, const fc::ecc::private_key& key)
{
  smt_create_operation op0;
  smt_create_operation op1;
  smt_create_operation op2;

  try
  {
    fund( control_account_name, ASSET( "5000.000 TESTS" ) );
    fund( control_account_name, ASSET( "5000.000 TBD" ) );
    this->generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    set_create_op( &op0, control_account_name, 0, *this->db );
    set_create_op( &op1, control_account_name, 1, *this->db );
    set_create_op( &op2, control_account_name, 1, *this->db );

    signed_transaction tx;
    tx.operations.push_back( op0 );
    tx.operations.push_back( op1 );
    tx.operations.push_back( op2 );
    tx.set_expiration( this->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    this->push_transaction( tx, key, 0, hive::protocol::serialization_mode_controller::get_current_pack() );

    this->generate_block();

    std::array<asset_symbol_type, 3> retVal;
    retVal[0] = op0.symbol;
    retVal[1] = op1.symbol;
    retVal[2] = op2.symbol;
    std::sort(retVal.begin(), retVal.end(),
        [](const asset_symbol_type & a, const asset_symbol_type & b) -> bool
    {
      return a.to_nai() < b.to_nai();
    });
    return retVal;
  }
  FC_LOG_AND_RETHROW();
}

template< typename T >
void t_smt_database_fixture< T >::push_invalid_operation( const operation& invalid_op, const fc::ecc::private_key& key )
{
  signed_transaction tx;
  tx.operations.push_back( invalid_op );
  tx.set_expiration( this->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  HIVE_REQUIRE_THROW( this->push_transaction( tx, fc::ecc::private_key(), database::skip_transaction_dupe_check,
    hive::protocol::serialization_mode_controller::get_current_pack() ), fc::assert_exception );
}

template< typename T >
void t_smt_database_fixture< T >::create_invalid_smt( const char* control_account_name, const fc::ecc::private_key& key )
{
  // Fail due to precision too big.
  smt_create_operation op_precision;
  HIVE_REQUIRE_THROW( set_create_op( &op_precision, control_account_name, HIVE_ASSET_MAX_DECIMALS + 1, *this->db ), fc::assert_exception );
}

template< typename T >
void t_smt_database_fixture< T >::create_conflicting_smt( const asset_symbol_type existing_smt, const char* control_account_name,
  const fc::ecc::private_key& key )
{
  // Fail due to the same nai & precision.
  smt_create_operation op_same;
  set_create_op( &op_same, control_account_name, existing_smt.to_nai(), existing_smt.decimals(), *this->db );
  push_invalid_operation( op_same, key );
  // Fail due to the same nai (though different precision).
  smt_create_operation op_same_nai;
  set_create_op( &op_same_nai, control_account_name, existing_smt.to_nai(), existing_smt.decimals() == 0 ? 1 : 0, *this->db );
  push_invalid_operation( op_same_nai, key );
}

template< typename T >
smt_generation_unit t_smt_database_fixture< T >::get_generation_unit( const units& hive_unit, const units& token_unit )
{
  smt_generation_unit ret;

  ret.hive_unit = hive_unit;
  ret.token_unit = token_unit;

  return ret;
}

template< typename T >
smt_capped_generation_policy t_smt_database_fixture< T >::get_capped_generation_policy
(
  const smt_generation_unit& pre_soft_cap_unit,
  const smt_generation_unit& post_soft_cap_unit,
  uint16_t soft_cap_percent,
  uint32_t min_unit_ratio,
  uint32_t max_unit_ratio
)
{
  smt_capped_generation_policy ret;

  ret.pre_soft_cap_unit = pre_soft_cap_unit;
  ret.post_soft_cap_unit = post_soft_cap_unit;

  ret.soft_cap_percent = soft_cap_percent;

  ret.min_unit_ratio = min_unit_ratio;
  ret.max_unit_ratio = max_unit_ratio;

  return ret;
}

template asset_symbol_type t_smt_database_fixture< clean_database_fixture >::create_smt( const string& account_name, const fc::ecc::private_key& key, uint8_t token_decimal_places );

template asset_symbol_type t_smt_database_fixture< hived_fixture >::create_smt( const string& account_name, const fc::ecc::private_key& key, uint8_t token_decimal_places );

template void t_smt_database_fixture< clean_database_fixture >::create_invalid_smt( const char* control_account_name, const fc::ecc::private_key& key );
template void t_smt_database_fixture< clean_database_fixture >::create_conflicting_smt( const asset_symbol_type existing_smt, const char* control_account_name, const fc::ecc::private_key& key );
template std::array<asset_symbol_type, 3> t_smt_database_fixture< clean_database_fixture >::create_smt_3( const char* control_account_name, const fc::ecc::private_key& key );

template smt_generation_unit t_smt_database_fixture< clean_database_fixture >::get_generation_unit( const units& hive_unit, const units& token_unit );
template smt_capped_generation_policy t_smt_database_fixture< clean_database_fixture >::get_capped_generation_policy
(
  const smt_generation_unit& pre_soft_cap_unit,
  const smt_generation_unit& post_soft_cap_unit,
  uint16_t soft_cap_percent,
  uint32_t min_unit_ratio,
  uint32_t max_unit_ratio
);

#endif


delayed_vote_database_fixture::delayed_vote_database_fixture( uint16_t shared_file_size_in_mb )
  : config_database_fixture( [](){ set_mainnet_cashout_values( false ); set_mainnet_feed_values( false ); }, shared_file_size_in_mb )
{}

delayed_vote_database_fixture::~delayed_vote_database_fixture()
{
  configuration_data.reset_cashout_values();
  configuration_data.reset_feed_values();
}

void delayed_vote_database_fixture::witness_vote( const std::string& account, const std::string& witness, const bool approve, const fc::ecc::private_key& key )
{
  account_witness_vote_operation op;

  op.account = account;
  op.witness = witness;
  op.approve = approve;

  push_transaction( op, key );
}

time_point_sec delayed_vote_database_fixture::move_forward_with_update( const fc::microseconds& time, delayed_voting::opt_votes_update_data_items& items )
{
  std::vector<std::pair<account_name_type,votes_update_data> > tmp;
  tmp.reserve( items->size() );

  for(const votes_update_data& var : *items)
    tmp.emplace_back(var.account->get_name(), var);

  items->clear();

  generate_blocks( db->head_block_time() + time );

  for(const auto& var : tmp)
  {
    auto x = var.second;
    x.account = &db->get_account(var.first);
    items->insert(x);
  }

    return db->head_block_time();
};

int32_t delayed_vote_database_fixture::get_user_voted_witness_count( const account_name_type& name )
{
  int32_t res = 0;

  const auto& vidx = db->get_index< witness_vote_index >().indices().get<by_account_witness>();
  auto itr = vidx.lower_bound( boost::make_tuple( name, account_name_type() ) );
  while( itr != vidx.end() && itr->account == name )
  {
    ++itr;
    ++res;
  }
  return res;
}

asset delayed_vote_database_fixture::to_vest( const asset& liquid, const bool to_reward_balance )
{
  const auto& cprops = db->get_dynamic_global_properties();
  price vesting_share_price = to_reward_balance ? cprops.get_reward_vesting_share_price() : cprops.get_vesting_share_price();

  return liquid * ( vesting_share_price );
}

template< typename COLLECTION >
fc::optional< size_t > delayed_vote_database_fixture::get_position_in_delayed_voting_array( const COLLECTION& collection, size_t nr_interval, size_t seconds )
{
  if( collection.empty() )
    return fc::optional< size_t >();

  auto time = collection[ 0 ].time + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS * nr_interval ) + fc::seconds( seconds );

  size_t idx = 0;
  for( auto& item : collection )
  {
    auto end_of_day = item.time + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS );

    if( end_of_day > time )
      return idx;

    ++idx;
  }

  return fc::optional< size_t >();
}

template< typename COLLECTION >
bool delayed_vote_database_fixture::check_collection( const COLLECTION& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val )
{
  if( idx.value >= collection.size() )
    return false;
  else
  {
    bool check_time = collection[ idx.value ].time == time;
    bool check_val = collection[ idx.value ].val == val;
    return check_time && check_val;
  }
}

template< typename COLLECTION >
bool delayed_vote_database_fixture::check_collection( const COLLECTION& collection, const bool withdraw_executor, const share_type val, const account_object& obj )
{
  auto found = collection->find( { withdraw_executor, val, &obj } );
  if( found == collection->end() )
    return false;

  if( !found->account )
    return false;
  return ( found->withdraw_executor == withdraw_executor ) && ( found->val == val ) && ( found->account->get_id() == obj.get_id() );
}

using dvd_vector = std::vector< delayed_votes_data >;
using bip_dvd_vector = chainbase::t_vector< delayed_votes_data >;

template fc::optional< size_t > delayed_vote_database_fixture::get_position_in_delayed_voting_array< bip_dvd_vector >( const bip_dvd_vector& collection, size_t day, size_t minutes );
template bool delayed_vote_database_fixture::check_collection< dvd_vector >( const dvd_vector& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val );
#ifndef ENABLE_STD_ALLOCATOR
template bool delayed_vote_database_fixture::check_collection< bip_dvd_vector >( const bip_dvd_vector& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val );
#endif
template bool delayed_vote_database_fixture::check_collection< delayed_voting::opt_votes_update_data_items >( const delayed_voting::opt_votes_update_data_items& collection, const bool withdraw_executor, const share_type val, const account_object& obj );

} } // hive::chain
