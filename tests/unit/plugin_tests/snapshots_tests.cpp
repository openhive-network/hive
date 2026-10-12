#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/account_object.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/plugins/chain/snapshot_load_marker.hpp>
#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include "../db_fixture/snapshots_fixture.hpp"
#include "../db_fixture/hived_fixture.hpp"

#include <fc/crypto/sha256.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>

#include <rocksdb/db.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string_view>

using namespace hive::chain;
using namespace hive::protocol;
using hive::plugins::chain::snapshot_load_marker;

namespace {

using manifest_column_families_t = std::map< std::string, ::rocksdb::ColumnFamilyHandle* >;

/// Opens the snapshot manifest for writing and passes it, with its column families by name, to `modify`.
void modify_snapshot_manifest( const fc::path& manifest_dir, const std::function< void( ::rocksdb::DB&, const manifest_column_families_t& ) >& modify )
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

  manifest_column_families_t column_families;
  for( size_t i = 0; i < cf_names.size(); ++i )
    column_families[ cf_names[ i ] ] = cf_handles[ i ];

  modify( *manifest_db, column_families );

  for( auto* handle : cf_handles )
    BOOST_REQUIRE( manifest_db->DestroyColumnFamilyHandle( handle ).ok() );
  BOOST_REQUIRE( manifest_db->Close().ok() );
}

std::string get_manifest_entry( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families, const std::string& column_family, const std::string& key )
{
  BOOST_REQUIRE( column_families.count( column_family ) );
  std::string value;
  BOOST_REQUIRE( manifest_db.Get( ::rocksdb::ReadOptions(), column_families.at( column_family ), key, &value ).ok() );
  return value;
}

void put_manifest_entry( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families, const std::string& column_family, const std::string& key, const std::string& value )
{
  BOOST_REQUIRE( column_families.count( column_family ) );
  BOOST_REQUIRE( manifest_db.Put( ::rocksdb::WriteOptions(), column_families.at( column_family ), key, value ).ok() );
}

/// Changes the memory layout checksum of one type in the state definitions stored in the snapshot manifest; returns the name of that type.
std::string alter_state_definitions_in_snapshot( const fc::path& manifest_dir )
{
  std::string type_name;
  modify_snapshot_manifest( manifest_dir, [&]( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families )
  {
    const std::string state_definitions = get_manifest_entry( manifest_db, column_families, "STATE_DEFINITIONS_DATA", "STATE_DEFINITIONS_DATA" );

    fc::variants types = fc::json::from_string( state_definitions, fc::json::format_validation_mode::full ).get_array();
    BOOST_REQUIRE( !types.empty() );
    fc::mutable_variant_object altered_type( types.front().get_object() );
    type_name = altered_type[ "name" ].as_string();
    altered_type[ "checksum" ] = altered_type[ "checksum" ].as_string() + "0";
    types.front() = fc::variant( altered_type );

    put_manifest_entry( manifest_db, column_families, "STATE_DEFINITIONS_DATA", "STATE_DEFINITIONS_DATA", fc::json::to_string( types ) );
  } );
  return type_name;
}

/// Changes the type of the first member of one struct in the state serialization stored in the snapshot manifest; returns the names of that struct and member.
std::pair< std::string, std::string > alter_state_serialization_in_snapshot( const fc::path& manifest_dir )
{
  std::pair< std::string, std::string > altered;
  modify_snapshot_manifest( manifest_dir, [&]( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families )
  {
    const std::string state_serialization = get_manifest_entry( manifest_db, column_families, "STATE_DEFINITIONS_DATA", "STATE_SERIALIZATION_DATA" );

    auto types = fc::json::from_string( state_serialization, fc::json::format_validation_mode::full ).as< std::vector< util::serialized_type_data > >();
    auto type_it = std::find_if( types.begin(), types.end(), []( const util::serialized_type_data& type ) { return type.kind == "struct" && !type.entries.empty(); } );
    BOOST_REQUIRE( type_it != types.end() );
    type_it->entries.front().value += "_altered";
    altered = { type_it->name, type_it->entries.front().name };

    put_manifest_entry( manifest_db, column_families, "STATE_DEFINITIONS_DATA", "STATE_SERIALIZATION_DATA", fc::json::to_string( types ) );
  } );
  return altered;
}

/// Turns the snapshot into one of the format preceding serialization descriptors (memory layout definitions only).
void make_layout_only_snapshot( const fc::path& manifest_dir )
{
  modify_snapshot_manifest( manifest_dir, [&]( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families )
  {
    put_manifest_entry( manifest_db, column_families, "IRREVERSIBLE_STATE", "SNAPSHOT_VERSION", "2.3" );
    BOOST_REQUIRE( manifest_db.Delete( ::rocksdb::WriteOptions(), column_families.at( "STATE_DEFINITIONS_DATA" ), "STATE_SERIALIZATION_DATA" ).ok() );
  } );
}

void set_state_serialization_descriptor_version_in_snapshot( const fc::path& manifest_dir, const std::string& version )
{
  modify_snapshot_manifest( manifest_dir, [&]( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families )
  {
    put_manifest_entry( manifest_db, column_families, "STATE_DEFINITIONS_DATA", "STATE_SERIALIZATION_DESCRIPTOR_VERSION", version );
  } );
}

/// Position of a template value argument spelled as by a demangler - with a literal suffix (`33ul') or a cast (`(char)1'), npos if there is none.
size_t find_demangler_spelling( const std::string& names )
{
  for( size_t pos = 0; pos < names.size(); ++pos )
  {
    if( names[ pos ] == '(' )
      return pos;
    if( names[ pos ] < '0' || names[ pos ] > '9' )
      continue;

    size_t end = pos + 1;
    while( end < names.size() && std::string_view( "uUlL" ).find( names[ end ] ) != std::string_view::npos )
      ++end;
    if( end > pos + 1 && end < names.size() && ( names[ end ] == ',' || names[ end ] == '>' ) )
      return pos;
  }
  return std::string::npos;
}

std::string read_state_serialization_from_snapshot( const fc::path& manifest_dir )
{
  std::string state_serialization;
  modify_snapshot_manifest( manifest_dir, [&]( ::rocksdb::DB& manifest_db, const manifest_column_families_t& column_families )
  {
    state_serialization = get_manifest_entry( manifest_db, column_families, "STATE_DEFINITIONS_DATA", "STATE_SERIALIZATION_DATA" );
  } );
  return state_serialization;
}

/// Starts the node with all witnesses and produces enough blocks to make some of them irreversible, so they reach the block log (replayed after a snapshot load).
void start_node_with_all_witnesses( snapshots_fixture& fixture )
{
  fixture.postponed_init();
  fixture.generate_block();
  fixture.db()->set_hardfork( 24 );
  fixture.generate_block();

  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    fixture.account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), fixture.init_account_pub_key );
    fixture.fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
    fixture.witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), fixture.init_account_priv_key, "foo.bar", fixture.init_account_pub_key, HIVE_MIN_PRODUCER_REWARD );
  }
  fixture.validate_database();
  // produces blocks until the feed is updated, by which time the block log holds many irreversible blocks
  fixture.set_price_feed( HBD_price( 1000, 1000 ) );
  fixture.generate_block();
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

const hived_fixture::config_line_t load_snapshot_if_no_state{ "load-snapshot-if-no-state", { "snap" } };

fc::path state_dir()
{
  return hive::utilities::temp_directory_path() / "blockchain";
}

std::optional< snapshot_load_marker::pending_load > read_snapshot_load_marker()
{
  return snapshot_load_marker( state_dir() ).read();
}

/// Restarts the node with the state_snapshot plugin reading snapshots from `snapshot_root_dir`, and with the given options.
void restart_with_snapshot_plugin( snapshots_fixture& fixture, const std::string& snapshot_root_dir, hived_fixture::config_arg_override_t options )
{
  fixture.reset_fixture( false );
  options.push_back( hived_fixture::config_line_t( { "plugin", { "state_snapshot" } } ) );
  options.push_back( hived_fixture::config_line_t( { "snapshot-root-dir", { snapshot_root_dir } } ) );
  hive::plugins::state_snapshot::state_snapshot_plugin* plugin = nullptr;
  fixture.postponed_init( options, &plugin );
}

/// Restarts the node without any load option and returns its head block, which is then also the head of the block log.
uint32_t restart_and_get_head_block_num( snapshots_fixture& fixture )
{
  fixture.reset_fixture( false );
  fixture.postponed_init();
  return fixture.db()->head_block_num();
}

/// Checks that `start` throws an fc::exception whose details contain all `fragments`, and that hived takes it for a refusal a restart cannot fix.
void check_startup_refused( snapshots_fixture& fixture, const std::function< void() >& start, const std::vector< std::string >& fragments )
{
  BOOST_CHECK_EXCEPTION( start(), fc::exception, [&]( const fc::exception& e )
  {
    const std::string details = e.to_detail_string();
    return std::all_of( fragments.begin(), fragments.end(), [&]( const std::string& fragment ) { return details.find( fragment ) != std::string::npos; } );
  } );
  BOOST_CHECK( fixture.get_chain_plugin().startup_refused() );
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
    const auto [ altered_type, altered_member ] =
      alter_state_serialization_in_snapshot( data_dir / snapshot_root_dir / "snap" / "snapshot-manifest" );

    // closes the node, so its state files are complete
    reset_fixture( false );

    const fc::path state_dir = data_dir / "blockchain";
    const auto state_files_before = hash_state_files( state_dir );

    const auto names_altered_member = [ &altered_type = altered_type, &altered_member = altered_member ]( const shm_state_definitions_mismatch_exception& e )
    {
      const std::string details = e.to_detail_string();
      return details.find( "Type: " + altered_type + " is serialized differently" ) != std::string::npos &&
             details.find( "`" + altered_member + ": " ) != std::string::npos;
    };
    BOOST_CHECK_EXCEPTION( load_snapshot( snapshot_root_dir ), shm_state_definitions_mismatch_exception, names_altered_member );
    reset_fixture( false );

    BOOST_REQUIRE( state_files_before == hash_state_files( state_dir ) );

    // a state recreated with the snapshot's definitions would fail to open
    BOOST_REQUIRE_NO_THROW( postponed_init() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( snapshot_load_ignores_memory_layout )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: snapshot_load_ignores_memory_layout" );

    const std::string snapshot_root_dir = "snapshot_load_ignores_memory_layout";
    clear_snapshot( snapshot_root_dir );

    start_node_with_all_witnesses( *this );
    dump_snapshot( snapshot_root_dir );
    const uint32_t head_block_num = db()->head_block_num();

    // as if the snapshot came from a build whose memory layout of some type differs, e.g. because of other Boost version
    const fc::path data_dir = hive::utilities::temp_directory_path();
    alter_state_definitions_in_snapshot( data_dir / snapshot_root_dir / "snap" / "snapshot-manifest" );

    BOOST_REQUIRE_NO_THROW( load_snapshot( snapshot_root_dir ) );
    BOOST_REQUIRE_GE( db()->head_block_num(), head_block_num );

    // the loaded state is stamped with definitions of this binary, so it opens with the strict memory layout check
    reset_fixture( false );
    BOOST_REQUIRE_NO_THROW( postponed_init() );
    BOOST_REQUIRE_GE( db()->head_block_num(), head_block_num );
    generate_block();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( layout_only_snapshot_is_checked_by_memory_layout )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: layout_only_snapshot_is_checked_by_memory_layout" );

    const std::string snapshot_root_dir = "layout_only_snapshot_is_checked_by_memory_layout";
    clear_snapshot( snapshot_root_dir );

    start_node_with_all_witnesses( *this );
    dump_snapshot( snapshot_root_dir );
    const uint32_t head_block_num = db()->head_block_num();

    const fc::path manifest_dir = hive::utilities::temp_directory_path() / snapshot_root_dir / "snap" / "snapshot-manifest";
    make_layout_only_snapshot( manifest_dir );

    BOOST_REQUIRE_NO_THROW( load_snapshot( snapshot_root_dir ) );
    BOOST_REQUIRE_GE( db()->head_block_num(), head_block_num );

    const std::string altered_type = alter_state_definitions_in_snapshot( manifest_dir );
    BOOST_CHECK_EXCEPTION( load_snapshot( snapshot_root_dir ), shm_state_definitions_mismatch_exception,
      [&]( const shm_state_definitions_mismatch_exception& e )
      {
        return e.to_detail_string().find( altered_type ) != std::string::npos;
      } );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( snapshot_state_serialization_uses_canonical_names )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: snapshot_state_serialization_uses_canonical_names" );

    const std::string snapshot_root_dir = "snapshot_state_serialization_uses_canonical_names";
    clear_snapshot( snapshot_root_dir );

    postponed_init();
    generate_block();
    dump_snapshot( snapshot_root_dir );

    const std::string state_serialization =
      read_state_serialization_from_snapshot( hive::utilities::temp_directory_path() / snapshot_root_dir / "snap" / "snapshot-manifest" );

    // nothing specific to Boost version, standard library, demangler or platform
    for( const std::string& fragment : { "allocator", "segment_manager", "std::less", "unsigned long", "long long", "__cxx11", " const", "long double" } )
      BOOST_CHECK_MESSAGE( state_serialization.find( fragment ) == std::string::npos, "State serialization contains: " << fragment );
    // template value arguments as spelled by a demangler: with literal suffixes (`33ul') or casts (`(char)1')
    const size_t demangler_spelling = find_demangler_spelling( state_serialization );
    BOOST_CHECK_MESSAGE( demangler_spelling == std::string::npos,
      "State serialization contains: " << state_serialization.substr( demangler_spelling == std::string::npos ? 0 : demangler_spelling, 20 ) );

    const auto types = fc::json::from_string( state_serialization, fc::json::format_validation_mode::full ).as< std::vector< util::serialized_type_data > >();
    const auto feed_history = std::find_if( types.begin(), types.end(),
      []( const util::serialized_type_data& type ) { return type.name == "hive::chain::feed_history_object"; } );
    BOOST_REQUIRE( feed_history != types.end() );
    BOOST_CHECK_EQUAL( feed_history->kind, "struct" );

    const auto price_history = std::find_if( feed_history->entries.begin(), feed_history->entries.end(),
      []( const util::serialized_type_data::entry& member ) { return member.name == "price_history"; } );
    BOOST_REQUIRE( price_history != feed_history->entries.end() );
    BOOST_CHECK_EQUAL( price_history->value, "boost::container::deque<" + util::canonical_type_name< HBD_price >() + ">" );
    BOOST_CHECK_EQUAL( util::canonical_type_name< HBD_price >(),
      "hive::protocol::tiny_price<" + std::to_string( HIVE_ASSET_NUM_HBD ) + ", " + std::to_string( HIVE_ASSET_NUM_HIVE ) + ">" );

    // leaf types are described by their encoding
    const auto feed_history_id = std::find_if( types.begin(), types.end(),
      []( const util::serialized_type_data& type ) { return type.name == "chainbase::oid<hive::chain::feed_history_object>"; } );
    BOOST_REQUIRE( feed_history_id != types.end() );
    BOOST_CHECK_EQUAL( feed_history_id->kind, "leaf" );
    const util::serialized_type_data::entries_vector_t id_encoding{ { "encoding", "uint32_t" } };
    BOOST_CHECK( feed_history_id->entries == id_encoding );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( snapshot_with_other_descriptor_version_is_refused )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: snapshot_with_other_descriptor_version_is_refused" );

    const std::string snapshot_root_dir = "snapshot_with_other_descriptor_version_is_refused";
    clear_snapshot( snapshot_root_dir );

    postponed_init();
    generate_block();
    dump_snapshot( snapshot_root_dir );

    const std::string current_version = std::to_string( util::STATE_SERIALIZATION_DESCRIPTOR_VERSION );
    const std::string other_version = std::to_string( util::STATE_SERIALIZATION_DESCRIPTOR_VERSION + 1 );
    const fc::path manifest_dir = hive::utilities::temp_directory_path() / snapshot_root_dir / "snap" / "snapshot-manifest";
    set_state_serialization_descriptor_version_in_snapshot( manifest_dir, other_version );
    // descriptors of other rules differ too - the version is compared before them
    alter_state_serialization_in_snapshot( manifest_dir );

    BOOST_CHECK_EXCEPTION( load_snapshot( snapshot_root_dir ), fc::assert_exception,
      [&]( const fc::assert_exception& e )
      {
        const std::string details = e.to_detail_string();
        return details.find( "State serialization descriptor version mismatch - snapshot describes its state with version " + other_version +
                             ", this build with version " + current_version ) != std::string::npos;
      } );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( load_snapshot_if_no_state_loads_only_without_state )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: load_snapshot_if_no_state_loads_only_without_state" );

    const std::string snapshot_root_dir = "load_snapshot_if_no_state_loads_only_without_state";
    clear_snapshot( snapshot_root_dir );

    start_node_with_all_witnesses( *this );
    dump_snapshot( snapshot_root_dir );
    const uint32_t snapshot_head_block_num = db()->head_block_num();
    // with all witnesses active blocks become irreversible, and reach the block log, about 15 blocks later
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    const uint32_t head_block_num = restart_and_get_head_block_num( *this );
    BOOST_REQUIRE_GT( head_block_num, snapshot_head_block_num );

    // a complete state is neither wiped nor replaced
    restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state } );
    BOOST_CHECK( !db()->get_snapshot_loaded() );
    BOOST_CHECK_EQUAL( db()->head_block_num(), head_block_num );
    BOOST_CHECK( !read_snapshot_load_marker() );

    // without a state the snapshot is loaded, and the block log replayed forward to its head
    reset_fixture( false );
    BOOST_REQUIRE( fc::remove( state_dir() / "shared_memory.bin" ) );
    restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state } );
    BOOST_CHECK( db()->get_snapshot_loaded() );
    BOOST_CHECK_EQUAL( db()->head_block_num(), head_block_num );
    BOOST_CHECK( !read_snapshot_load_marker() );

    // the snapshot is not even looked at when the state is complete
    clear_snapshot( snapshot_root_dir );
    restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state } );
    BOOST_CHECK( !db()->get_snapshot_loaded() );
    BOOST_CHECK_EQUAL( db()->head_block_num(), head_block_num );
    generate_block();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( interrupted_snapshot_load_is_loaded_again )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: interrupted_snapshot_load_is_loaded_again" );

    const std::string snapshot_root_dir = "interrupted_snapshot_load_is_loaded_again";
    clear_snapshot( snapshot_root_dir );

    start_node_with_all_witnesses( *this );
    dump_snapshot( snapshot_root_dir );
    const uint32_t head_block_num = restart_and_get_head_block_num( *this );
    reset_fixture( false );

    // as left by a load killed while its indices were being loaded
    snapshot_load_marker( state_dir() ).write( snapshot_load_marker::load_stage::loading, "snap" );

    const std::vector< hived_fixture::config_arg_override_t > starts_without_load =
      { {}, { hived_fixture::config_line_t( { "replay-blockchain", { "" } } ) } };
    for( const auto& options : starts_without_load )
    {
      reset_fixture( false );
      check_startup_refused( *this, [&]() { postponed_init( options ); },
        { "The load of snapshot `snap' did not complete", "--load-snapshot-if-no-state", "--force-replay" } );
      reset_fixture( false );
      BOOST_REQUIRE( read_snapshot_load_marker() );
    }

    restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state } );
    BOOST_CHECK( db()->get_snapshot_loaded() );
    BOOST_CHECK_EQUAL( db()->head_block_num(), head_block_num );
    BOOST_CHECK( !read_snapshot_load_marker() );
    generate_block();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( interrupted_replay_after_snapshot_load_is_resumed )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: interrupted_replay_after_snapshot_load_is_resumed" );

    const std::string snapshot_root_dir = "interrupted_replay_after_snapshot_load_is_resumed";
    clear_snapshot( snapshot_root_dir );

    start_node_with_all_witnesses( *this );
    dump_snapshot( snapshot_root_dir );
    const uint32_t snapshot_head_block_num = db()->head_block_num();
    // with all witnesses active blocks become irreversible, and reach the block log, about 15 blocks later
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    const uint32_t head_block_num = restart_and_get_head_block_num( *this );
    const uint32_t stop_block_num = snapshot_head_block_num + 2;
    BOOST_REQUIRE_GT( head_block_num, stop_block_num );

    reset_fixture( false );
    BOOST_REQUIRE( fc::remove( state_dir() / "shared_memory.bin" ) );
    restart_with_snapshot_plugin( *this, snapshot_root_dir,
      { load_snapshot_if_no_state, hived_fixture::config_line_t( { "stop-at-block", { std::to_string( stop_block_num ) } } ) } );
    BOOST_REQUIRE_EQUAL( db()->head_block_num(), stop_block_num );
    const auto marker = read_snapshot_load_marker();
    BOOST_REQUIRE( marker );
    BOOST_CHECK( marker->stage == snapshot_load_marker::load_stage::replaying );
    BOOST_CHECK_EQUAL( marker->snapshot, "snap" );

    // the replay is resumed, not the snapshot loaded again: it is gone
    clear_snapshot( snapshot_root_dir );
    restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state } );
    BOOST_CHECK_EQUAL( db()->head_block_num(), head_block_num );
    BOOST_CHECK( !read_snapshot_load_marker() );

    // and a normal start finds a complete state
    BOOST_CHECK_EQUAL( restart_and_get_head_block_num( *this ), head_block_num );
    generate_block();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( startup_refusals_keep_state )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: startup_refusals_keep_state" );

    const std::string snapshot_root_dir = "startup_refusals_keep_state";
    clear_snapshot( snapshot_root_dir );

    start_node_with_all_witnesses( *this );
    dump_snapshot( snapshot_root_dir );
    const uint32_t snapshot_head_block_num = db()->head_block_num();
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    reset_fixture( false );
    const auto state_files_before = hash_state_files( state_dir() );

    for( const std::string rebuild_option : { "force-replay", "resync-blockchain" } )
    {
      check_startup_refused( *this,
        [&]() { restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state, hived_fixture::config_line_t( { rebuild_option, { "" } } ) } ); },
        { "--load-snapshot-if-no-state cannot be combined" } );
      reset_fixture( false );
      BOOST_REQUIRE( state_files_before == hash_state_files( state_dir() ) );
    }

    // a state behind the block log, as an interrupted replay after a load left it before the marker existed, is refused by throwing
    restart_with_snapshot_plugin( *this, snapshot_root_dir, { hived_fixture::config_line_t( { "load-snapshot", { "snap" } } ),
      hived_fixture::config_line_t( { "stop-at-block", { std::to_string( snapshot_head_block_num + 1 ) } } ) } );
    BOOST_REQUIRE_EQUAL( db()->head_block_num(), snapshot_head_block_num + 1 );
    reset_fixture( false );
    snapshot_load_marker( state_dir() ).clear();
    check_startup_refused( *this, [&]() { postponed_init(); }, { "Headblock and statefile are inconsistent" } );
    reset_fixture( false );

    // a snapshot refused by its checks is refused on every start, before anything is wiped
    alter_state_serialization_in_snapshot( hive::utilities::temp_directory_path() / snapshot_root_dir / "snap" / "snapshot-manifest" );
    snapshot_load_marker( state_dir() ).write( snapshot_load_marker::load_stage::loading, "snap" );
    const auto state_files_with_marker = hash_state_files( state_dir() );
    for( int i = 0; i < 2; ++i )
    {
      BOOST_CHECK_THROW( restart_with_snapshot_plugin( *this, snapshot_root_dir, { load_snapshot_if_no_state } ), shm_state_definitions_mismatch_exception );
      BOOST_CHECK( get_chain_plugin().startup_refused() );
      reset_fixture( false );
      BOOST_REQUIRE( state_files_with_marker == hash_state_files( state_dir() ) );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
