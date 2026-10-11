#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/account_object.hpp>

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include "../db_fixture/snapshots_fixture.hpp"
#include "../db_fixture/hived_fixture.hpp"

#include <fc/crypto/sha256.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>

#include <rocksdb/db.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>

using namespace hive::chain;
using namespace hive::protocol;

namespace {

/// Changes the checksum of one type in the state definitions stored in the snapshot manifest; returns the name of that type.
std::string alter_state_definitions_in_snapshot( const fc::path& manifest_dir )
{
  std::vector< std::string > cf_names;
  auto status = ::rocksdb::DB::ListColumnFamilies( ::rocksdb::DBOptions(), manifest_dir.string(), &cf_names );
  BOOST_REQUIRE( status.ok() );

  std::vector< ::rocksdb::ColumnFamilyDescriptor > cf_descriptors;
  for( const auto& name : cf_names )
    cf_descriptors.emplace_back( name, ::rocksdb::ColumnFamilyOptions() );

  std::vector< ::rocksdb::ColumnFamilyHandle* > cf_handles;
  ::rocksdb::DB* raw_db = nullptr;
  status = ::rocksdb::DB::Open( ::rocksdb::Options(), manifest_dir.string(), cf_descriptors, &cf_handles, &raw_db );
  BOOST_REQUIRE( status.ok() );
  std::unique_ptr< ::rocksdb::DB > manifest_db( raw_db );

  const auto cf_it = std::find( cf_names.begin(), cf_names.end(), "STATE_DEFINITIONS_DATA" );
  BOOST_REQUIRE( cf_it != cf_names.end() );
  ::rocksdb::ColumnFamilyHandle* state_definitions_cf = cf_handles[ std::distance( cf_names.begin(), cf_it ) ];

  std::string state_definitions;
  status = manifest_db->Get( ::rocksdb::ReadOptions(), state_definitions_cf, "STATE_DEFINITIONS_DATA", &state_definitions );
  BOOST_REQUIRE( status.ok() );

  fc::variants types = fc::json::from_string( state_definitions, fc::json::format_validation_mode::full ).get_array();
  BOOST_REQUIRE( !types.empty() );
  fc::mutable_variant_object altered_type( types.front().get_object() );
  const std::string type_name = altered_type[ "name" ].as_string();
  altered_type[ "checksum" ] = altered_type[ "checksum" ].as_string() + "0";
  types.front() = fc::variant( altered_type );

  status = manifest_db->Put( ::rocksdb::WriteOptions(), state_definitions_cf, "STATE_DEFINITIONS_DATA", fc::json::to_string( types ) );
  BOOST_REQUIRE( status.ok() );

  for( auto* handle : cf_handles )
    BOOST_REQUIRE( manifest_db->DestroyColumnFamilyHandle( handle ).ok() );
  BOOST_REQUIRE( manifest_db->Close().ok() );
  return type_name;
}

/// Hashes of all files in the state directory (shared memory file, comment archive, block log), keyed by path.
std::map< std::string, fc::sha256 > hash_state_files( const fc::path& state_dir )
{
  BOOST_REQUIRE( fc::exists( state_dir / "shared_memory.bin" ) );
  BOOST_REQUIRE( fc::exists( state_dir / "comments-rocksdb-storage" ) );

  std::map< std::string, fc::sha256 > hashes;
  for( fc::recursive_directory_iterator it( state_dir ), end; it != end; ++it )
  {
    if( !fc::is_regular_file( *it ) )
      continue;
    std::ifstream file( ( *it ).string(), std::ios::binary );
    BOOST_REQUIRE( file.good() );
    const std::string content( ( std::istreambuf_iterator< char >( file ) ), std::istreambuf_iterator< char >() );
    hashes[ ( *it ).string() ] = fc::sha256::hash( content );
  }
  return hashes;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE( snapshots_tests, snapshots_fixture )

BOOST_AUTO_TEST_CASE( additional_allocation_after_snapshot_load )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: additional_allocation_after_snapshot_load" );

    clear_snapshot("additional_allocation_after_snapshot_load");
    {
      postponed_init(
        {
          hived_fixture::config_line_t({ "shared-file-size",
            { std::to_string(1024 * 1024 * hived_fixture::shared_file_size_small) } }
          )
        }
      );

      generate_block();
      db()->set_hardfork( 24 );
      generate_block();

      // Fill up the rest of the required miners
      for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
      {
        account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
        fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
        witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD );
      }
      validate_database();
      set_price_feed( HBD_price( 1000, 1000 ) );

      generate_block();

      const auto& index = db()->get_index<account_index>();
      const size_t initial_allocations = index.get_item_additional_allocation();

      ACTOR( NO_VESTING, alice );
      generate_block();
      ISSUE_FUNDS( "alice", HIVE_asset( 100'000'000 ) );
      BOOST_REQUIRE( compare_delayed_vote_count("alice", {}) );
      BOOST_REQUIRE_EQUAL(index.get_item_additional_allocation(), initial_allocations);

      vest( "alice", "alice", HIVE_asset( 100'000 ), alice_private_key );
      generate_block();
      BOOST_CHECK( compare_delayed_vote_count("alice", { static_cast<uint64_t>(get_vesting( "alice" ).amount.value) }) );
      BOOST_CHECK_GT(index.get_item_additional_allocation(), initial_allocations);
    }
    {
      dump_snapshot("additional_allocation_after_snapshot_load");

      generate_block();
      db()->set_hardfork( 24 );
      generate_block();
    }
    {
      load_snapshot("additional_allocation_after_snapshot_load");

      generate_block();
      db()->set_hardfork( 24 );
      generate_block();

      const auto& index = db()->get_index<account_index>();
      BOOST_REQUIRE_GT(index.get_item_additional_allocation(), 0);
    }

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( refused_snapshot_load_keeps_state )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: refused_snapshot_load_keeps_state" );

    const std::string snapshot_root_dir = "refused_snapshot_load_keeps_state";
    clear_snapshot( snapshot_root_dir );

    postponed_init();
    for( int i = 0; i < 5; ++i )
      generate_block();

    dump_snapshot( snapshot_root_dir );
    for( int i = 0; i < 3; ++i )
      generate_block();

    const fc::path data_dir = hive::utilities::temp_directory_path();
    const std::string altered_type =
      alter_state_definitions_in_snapshot( data_dir / snapshot_root_dir / "snap" / "snapshot-manifest" );

    // closes the node, so its state files are complete
    reset_fixture( false );

    const fc::path state_dir = data_dir / "blockchain";
    const auto state_files_before = hash_state_files( state_dir );

    BOOST_CHECK_EXCEPTION( load_snapshot( snapshot_root_dir ), shm_state_definitions_mismatch_exception,
      [&]( const shm_state_definitions_mismatch_exception& e )
      {
        return e.to_detail_string().find( altered_type ) != std::string::npos;
      } );
    reset_fixture( false );

    BOOST_REQUIRE( state_files_before == hash_state_files( state_dir ) );

    // a state recreated with the snapshot's definitions would fail to open
    BOOST_REQUIRE_NO_THROW( postponed_init() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
