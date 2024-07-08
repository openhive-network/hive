#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/block_storage_interface.hpp>
#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::plugins;

using config_line=hived_fixture::config_line_t;

BOOST_AUTO_TEST_SUITE(block_storage_tests)

#define INIT_FIXTURE_1( KEY, VALUE ) \
  fixture.postponed_init( \
    { \
      hived_fixture::config_line_t( { KEY, { VALUE } } ) \
    } \
  );

#define INIT_FIXTURE_2( KEY1, VALUE1, KEY2, VALUE2 ) \
  fixture.postponed_init( \
    { \
      hived_fixture::config_line_t( { KEY1, { VALUE1 } } ), \
      hived_fixture::config_line_t( { KEY2, { VALUE2 } } ) \
    } \
  );

void switch_block_storage( int initial_block_storage, int target_block_storage, 
                           bool clear_block_log, bool force_no_tail )
{
  // Get dedicated temporary directory for database (shm) files 
  // which should be preserved between hived (_fixture) runs.
  fc::temp_directory shm_dir( hive::utilities::temp_directory_path() );

  const int one_split_file_of_blocks = BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;

  uint32_t cutoff_height = 0;
  {
    // Set config file's block-log-split to provided value.
    hived_fixture fixture( true /*remove blockchain*/ );
    INIT_FIXTURE_2( "block-log-split", std::to_string( initial_block_storage ),
                    "shared-file-dir", shm_dir.path().string() );

    // Populate block log with two split files of blocks.
    for( int i = 0; i < one_split_file_of_blocks +1 ; ++i )
      fixture.generate_block();

    // Check irreversible head block as stored in block storage (block log)
    const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
    auto head_irreversible_block_storage = block_reader.head_block();
    // Check irreversible head block as stored in database (state)
    cutoff_height = fixture.db->get_last_irreversible_block_num();
    BOOST_REQUIRE_EQUAL( head_irreversible_block_storage->get_block_num(), cutoff_height );
    auto head_irreversible_block_db = fixture.db->get_last_irreversible_block_data();
    BOOST_REQUIRE_EQUAL( head_irreversible_block_storage->get_block_num(),
                          head_irreversible_block_db->get_block_num() );
    BOOST_REQUIRE_EQUAL( head_irreversible_block_storage->get_block_id(),
                          head_irreversible_block_db->get_block_id() );
    // Obviously head (and previous) blocks are stored in block log
    auto block_coh = block_reader.get_block_by_number( cutoff_height );
    BOOST_REQUIRE( block_coh );
    auto block_coh_minus_1 = block_reader.get_block_by_number( cutoff_height -1 );
    BOOST_REQUIRE( block_coh_minus_1 );
  }
  {
    // Set config file's block-log-split to provided value.
    // Remove block log generated in 1st run if needed.
    hived_fixture fixture( clear_block_log /*remove blockchain*/ );
    INIT_FIXTURE_2( "block-log-split", std::to_string( target_block_storage ),
                    "shared-file-dir", shm_dir.path().string() );

    // Verify that database was not cleared.
    BOOST_REQUIRE_EQUAL( cutoff_height, fixture.db->get_last_irreversible_block_num() );

    // Verify block storage contents.
    const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
    auto head_irreversible_block_storage = block_reader.head_block();
    BOOST_REQUIRE_EQUAL( head_irreversible_block_storage->get_block_num(), cutoff_height );
    // Verify that head block is available through api.
    auto block_coh = block_reader.get_block_by_number( cutoff_height );
    BOOST_REQUIRE( block_coh );
    if( force_no_tail )
    {
      // Check that there's only head block available (no tail at all).
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( cutoff_height -1 ), fc::assert_exception );
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( 1 ), fc::assert_exception );
    }
    else
    {
      auto below_head = block_reader.get_block_by_number( cutoff_height -1 );
      BOOST_REQUIRE( below_head );
      auto tail = block_reader.get_block_by_number( 1 );
      BOOST_REQUIRE( tail );
    }

    // Populate block log with a handful of blocks.
    const int additional_block_count = one_split_file_of_blocks +1;
    for( int i = 0; i < additional_block_count; ++i )
      fixture.generate_block();

    // Verify moving target.
    auto new_block = block_reader.get_block_by_number( cutoff_height + additional_block_count );
    BOOST_REQUIRE( new_block );
    if( force_no_tail )
    {
      // Check that there's only current head block available, earlier head is tail now.
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( cutoff_height ), fc::assert_exception );
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( cutoff_height +additional_block_count -1 ), fc::assert_exception );
    }
    else
    {
      auto below_head = block_reader.get_block_by_number( cutoff_height );
      BOOST_REQUIRE( below_head );
      below_head = block_reader.get_block_by_number( cutoff_height +additional_block_count -1 );
      BOOST_REQUIRE( below_head );
    }
  }
}

BOOST_AUTO_TEST_CASE( legacy_to_zero )
{
  try {
    ilog( "Testing switching from legacy monolithic block log to memory only storage." );

    // On 1st run use legacy monolithic block log (setting block-log-split to -1).
    // On 2nd run use memory-only single-block storage (setting block-log-split to 0).
    // Also remove block log generated on 1st run to be sure it will not be read.
    switch_block_storage( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/,
                          0 /*target_block_storage*/, 
                          true /*clear_block_log*/,
                          true /*force_no_tail*/ );

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( legacy_to_split )
{
  try {
    ilog( "Testing switching from legacy monolithic block log to split block log." );

    // On 1st run use legacy monolithic block log (setting block-log-split to -1).
    // On 2nd run use split block-log (using auto-split feature).
    // Make sure the log is not removed between runs.
    switch_block_storage( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/,
                          1 /*target_block_storage*/,
                          false /*clear_block_log*/,
                          false /*force_no_tail*/ );
    // The same for max part number.  
    switch_block_storage( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/,
                          MAX_FILES_OF_SPLIT_BLOCK_LOG /*target_block_storage*/,
                          false /*clear_block_log*/,
                          false /*force_no_tail*/ );

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( split_to_zero )
{
  try {
    ilog( "Testing switching from split block log to memory only storage." );

    // On 1st run use split block log (setting block-log-split to 9999).
    // On 2nd run use memory-only single-block storage (setting block-log-split to 0).
    // Also remove block log generated on 1st run to be sure it will not be read.
    switch_block_storage( MAX_FILES_OF_SPLIT_BLOCK_LOG /*initial_block_storage*/, 0 /*target_block_storage*/, 
                          true /*clear_block_log*/, true /*force_no_tail*/ );
    // The same for min part number.    
    switch_block_storage( 1 /*initial_block_storage*/, 0 /*target_block_storage*/, 
                          true /*clear_block_log*/, true /*force_no_tail*/ );

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( clean_replay_legacy_to_zero )
{
  try {
    ilog( "Testing switching from legacy to memory block storage with failed replay due to empty state." );

    // Get dedicated temporary directory for database (shm) files 
    // which is used to hide database from second (replay) run.
    fc::temp_directory shm_dir( hive::utilities::temp_directory_path() );

    {
      hived_fixture fixture( true /*remove blockchain*/ );
      INIT_FIXTURE_2( "block-log-split", std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG ),
                      "shared-file-dir", shm_dir.path().string() );

      for( int i = 0; i < BLOCKS_IN_SPLIT_BLOCK_LOG_FILE +1; ++i )
        fixture.generate_block();
    }
    {
      hived_fixture fixture( false /*remove blockchain*/ );
      // No blocks in block storage, cannot reindex an empty chain.
      BOOST_REQUIRE_THROW( INIT_FIXTURE_2( "block-log-split", "0", "replay-blockchain", "" ),
                           fc::assert_exception );
    }

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( clean_replay_legacy_to_split )
{
  try {
    ilog( "Testing switching from legacy to split block log with successful replay due to auto-split feature." );

    // Get dedicated temporary directory for database (shm) files 
    // which is used to hide database from second (replay) run.
    fc::temp_directory shm_dir( hive::utilities::temp_directory_path() );

    {
      hived_fixture fixture( true /*remove blockchain*/ );
      INIT_FIXTURE_2( "block-log-split", std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG ),
                      "shared-file-dir", shm_dir.path().string() );

      for( int i = 0; i < BLOCKS_IN_SPLIT_BLOCK_LOG_FILE +1; ++i )
        fixture.generate_block();
    }
    {
      hived_fixture fixture( false /*remove blockchain*/ );
      INIT_FIXTURE_2( "block-log-split", "1", "replay-blockchain", "" );
    }

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( replay_legacy_to_zero )
{
  try {
    ilog( "Testing switching from legacy to memory block storage with ineffective replay due to preserved state." );

    {
      hived_fixture fixture( true /*remove blockchain & state*/ );
      INIT_FIXTURE_1( "block-log-split", std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG ) );

      for( int i = 0; i < BLOCKS_IN_SPLIT_BLOCK_LOG_FILE +1; ++i )
        fixture.generate_block();
    }
    {
      hived_fixture fixture( false /*remove blockchain & state*/ );
      INIT_FIXTURE_2( "block-log-split", "0",
                      "replay-blockchain", "" );
    }

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( replay_legacy_to_split )
{
  try {
    ilog( "Testing switching from legacy to split block log with ineffective replay due to preserved state." );

    {
      hived_fixture fixture( true /*remove blockchain & state*/ );
      INIT_FIXTURE_1( "block-log-split", std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG ) );

      for( int i = 0; i < BLOCKS_IN_SPLIT_BLOCK_LOG_FILE +1; ++i )
        fixture.generate_block();
    }
    {
      hived_fixture fixture( false /*remove blockchain & state*/ );
      INIT_FIXTURE_2( "block-log-split", "1",
                      "replay-blockchain", "" );
    }

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

std::string get_block_log_part_path( fc::path& data_dir, uint32_t part_number )
{
  std::string part_path_str = ( data_dir.string() + "/blockchain/" ).c_str() +
    block_log_file_name_info::get_nth_part_file_name( part_number );
  return part_path_str;
}

void remove_block_log_part( fc::path& data_dir, uint32_t part_number )
{
  std::string part_path_str = get_block_log_part_path( data_dir, part_number );
  ilog( "Removing ${part_path_str}", (part_path_str) );
  fc::remove_all( part_path_str );
  part_path_str += block_log_file_name_info::_artifacts_extension;
  ilog( "Removing ${part_path_str}", (part_path_str) );
  fc::remove_all( part_path_str );
}

void require_blocks( hived_fixture& fixture, uint32_t block_num1, uint32_t block_num2 )
{
  const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
  auto block = block_reader.get_block_by_number( block_num1 );
  BOOST_REQUIRE( block );
  block = block_reader.get_block_by_number( block_num2 );
  BOOST_REQUIRE( block );
}

void init_fixture_and_check_blocks( int split_log_value, uint32_t block_num1, uint32_t block_num2 )
{
  hived_fixture fixture( false /*remove blockchain & state*/ );
  INIT_FIXTURE_1( "block-log-split", std::to_string( split_log_value ) );

  require_blocks( fixture, block_num1, block_num2 );
}

BOOST_AUTO_TEST_CASE( auto_split )
{
  try {
    ilog( "Testing block log auto-splitting feature of hived." );

    // We'll need 4 files of split block log (3 full + head block).
    const uint32_t blocks_count = 3*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE +1;

    fc::path data_dir;
    {
      // Generate source legacy log.
      hived_fixture fixture( true /*remove blockchain & state*/ );
      INIT_FIXTURE_1( "block-log-split", std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG ) );

      for( uint32_t i = 0; i < blocks_count; ++i )
        fixture.generate_block();

      data_dir = fixture.get_data_dir();
    }

    //---------------------------------------------------------------------------------------------
    // Standard scenarios with block log effectively NOT pruned, i.e. 1st block (log part) needed.

    // Auto-split legacy log (parts 1, 2 & 3 full of blocks, sole head block in part 4).
    init_fixture_and_check_blocks( 3, blocks_count, 1 );

    // Remove tail part of split log (including artifacts).
    remove_block_log_part( data_dir, 1 );
    // Auto-split missing tail part from legacy log.
    init_fixture_and_check_blocks( 3, blocks_count, 1 );

    // Remove mid part of split log (including artifacts).
    remove_block_log_part( data_dir, 2 );
    // Auto-split missing mid part from legacy log.
    init_fixture_and_check_blocks( 3, blocks_count, 1 );

    // Remove tail & mid part 3 of split log (including artifacts).
    remove_block_log_part( data_dir, 1 );
    remove_block_log_part( data_dir, 3 );
    // Auto-split won't start due to blocking mid part 2 (no overwrite policy)
    BOOST_REQUIRE_THROW( init_fixture_and_check_blocks( 3, blocks_count, 1 ),
                         fc::exception );

    // Refresh split log files.
    remove_block_log_part( data_dir, 2 );
    init_fixture_and_check_blocks( 3, blocks_count, 1 );

    // Remove head part of split log (including artifacts).
    remove_block_log_part( data_dir, 4 );
    // Auto-split won't start as generating "new" head part would make sense only if "current"
    // head part (#3) is full i.e. contains exactly BLOCKS_IN_SPLIT_BLOCK_LOG_FILE blocks. Which
    // is a very rare scenario (actually only due to manual removal of following part).
    BOOST_REQUIRE_THROW( init_fixture_and_check_blocks( 3, blocks_count, 1 ),
                         fc::assert_exception );

    // Clear remaining log parts.
    remove_block_log_part( data_dir, 1 );
    remove_block_log_part( data_dir, 2 );
    remove_block_log_part( data_dir, 3 );

    //---------------------------------------------------------------------------------------------
    // Standard scenarios with pruned block log, i.e. 1st block (log part) NOT needed.
    const uint32_t first_needed_block_num = BLOCKS_IN_SPLIT_BLOCK_LOG_FILE + 1;

    // Auto-split legacy log (parts 2 & 3 full of blocks, sole head block in part 4).
    init_fixture_and_check_blocks( 2, blocks_count, first_needed_block_num );
    // Check that unnecessary part 1 has not been generated.
    fc::path first_part_path( get_block_log_part_path( data_dir, 1 ) );
    BOOST_TEST_MESSAGE( first_part_path.string() );
    BOOST_REQUIRE( not fc::exists( first_part_path ) );

    // Remove tail part of split log (including artifacts).
    remove_block_log_part( data_dir, 2 );
    // Auto-split missing tail part from legacy log.
    init_fixture_and_check_blocks( 2, blocks_count, first_needed_block_num );
    // Check that unnecessary part 1 has not been generated.
    BOOST_REQUIRE( not fc::exists( first_part_path ) );

    // Remove mid part of split log (including artifacts).
    remove_block_log_part( data_dir, 3 );
    // Auto-split missing mid part from legacy log.
    init_fixture_and_check_blocks( 2, 3*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE, first_needed_block_num );
    // Check that unnecessary part 1 has not been generated.
    BOOST_REQUIRE( not fc::exists( first_part_path ) );

  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

void post_snapshot_block_storage_switch( int initial_block_storage, int target_block_storage, 
                                         bool force_no_tail )
{
  const int one_split_file_of_blocks = BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;

  // We'll need 4 files of split block log (3 full + head block).
  const uint32_t blocks_count = 3*one_split_file_of_blocks +1;

  fc::path data_dir;
  fc::path snapshot_root_dir;
  {
    // Generate initial log.
    hived_fixture fixture( true /*remove blockchain & state*/ );
    INIT_FIXTURE_2( "block-log-split", std::to_string( initial_block_storage ),
                    "plugin", "state_snapshot" );

    for( uint32_t i = 0; i < blocks_count; ++i )
      fixture.generate_block();

    data_dir = fixture.get_data_dir();
    snapshot_root_dir = data_dir / "snapshot_dir";
    fc::remove_all( snapshot_root_dir );
  }
  {
    // Dump snapshot with initial log.
    hived_fixture fixture( false /*remove blockchain & state*/ );
    hive::plugins::state_snapshot::state_snapshot_plugin* plugin = nullptr;
    fixture.postponed_init(
      {
        hived_fixture::config_line_t( { "block-log-split",
          { std::to_string( initial_block_storage ) } }
        ),
        hived_fixture::config_line_t( { "plugin",
          { "state_snapshot" } }
        ),
        hived_fixture::config_line_t( { "dump-snapshot",
          { std::string("snap") } }
        ),
        hived_fixture::config_line_t( { "snapshot-root-dir",
          { snapshot_root_dir.string() } }
        )
      },
      &plugin);
  }
  {
    // Load snapshot with target block storage
    hived_fixture fixture( true /*remove blockchain & state*/ );
    hive::plugins::state_snapshot::state_snapshot_plugin* plugin = nullptr;
    fixture.postponed_init(
      {
        hived_fixture::config_line_t( { "block-log-split",
          { std::to_string( target_block_storage ) } }
        ),
        hived_fixture::config_line_t( { "plugin",
          { "state_snapshot" } }
        ),
        hived_fixture::config_line_t( { "load-snapshot",
          { std::string("snap") } }
        ),
        hived_fixture::config_line_t( { "snapshot-root-dir",
          { snapshot_root_dir.string() } }
        )
      },
      &plugin);

    // Verify that database was restored.
    BOOST_REQUIRE_EQUAL( blocks_count, fixture.db->get_last_irreversible_block_num() );

    // Verify head block, kept even in memory-only block storage.
    const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
    auto head_irreversible_block_storage = block_reader.head_block();
    auto head_block_by_number = block_reader.get_block_by_number( blocks_count );
    BOOST_REQUIRE( head_block_by_number );
    BOOST_REQUIRE_EQUAL( head_block_by_number->get_block_id(), head_irreversible_block_storage->get_block_id() );
    BOOST_REQUIRE_EQUAL( head_block_by_number->get_block_num(), head_irreversible_block_storage->get_block_num() );
    BOOST_REQUIRE_EQUAL( head_irreversible_block_storage->get_block_num(), blocks_count );

    if( force_no_tail )
    {
      // Check that there's only head block available (no tail at all).
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( blocks_count -1 ), fc::assert_exception );
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( 1 ), fc::assert_exception );
    }
    else
    {
      auto below_head = block_reader.get_block_by_number( blocks_count -1 );
      BOOST_REQUIRE( below_head );
      auto tail = block_reader.get_block_by_number( 1 );
      BOOST_REQUIRE( tail );
    }

    // Populate block log with a handful of blocks.
    const int additional_block_count = one_split_file_of_blocks +1;
    for( int i = 0; i < additional_block_count; ++i )
      fixture.generate_block();

    // Verify moving target.
    auto new_block = block_reader.get_block_by_number( blocks_count + additional_block_count );
    BOOST_REQUIRE( new_block );
    if( force_no_tail )
    {
      // Check that there's only current head block available, earlier head is tail now.
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( blocks_count ), fc::assert_exception );
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( blocks_count +additional_block_count -1 ), fc::assert_exception );
    }
    else
    {
      auto below_head = block_reader.get_block_by_number( blocks_count );
      BOOST_REQUIRE( below_head );
      below_head = block_reader.get_block_by_number( blocks_count +additional_block_count -1 );
      BOOST_REQUIRE( below_head );
    }
  }
}

BOOST_AUTO_TEST_CASE( snapshot_legacy_to_zero )
{
  try {
    ilog( "Testing post-snapshot-load switching from legacy monolithic block log to memory only storage." );

    // Generate blocks & dump snapshot using legacy monolithic block log (setting block-log-split to -1).
    // Load snapshot using memory-only single-block storage (setting block-log-split to 0).
    post_snapshot_block_storage_switch( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/,
                                        0 /*target_block_storage*/, 
                                        true /*force_no_tail*/ );
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( snapshot_zero_to_zero )
{
  try {
    ilog( "Testing memory only storage setting pre-and-post-snapshot-load." );

    // Generate blocks & dump snapshot using memory-only single-block storage (setting block-log-split to 0).
    // Load snapshot using memory-only single-block storage (setting block-log-split to 0).
    post_snapshot_block_storage_switch( 0 /*initial_block_storage*/,
                                        0 /*target_block_storage*/, 
                                        true /*force_no_tail*/ );
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

typedef std::function<void(const fc::path& data_dir)> stage_processor_t;
typedef std::function<void(hived_fixture& fixture)> post_load_processor_t;
// Helper function supporting snapshot-load-incremental-replay scenarios with block storage switch.
void post_snapshot_replay_block_storage_switch( int initial_block_storage, 
  int target_block_storage, stage_processor_t post_generation_processor,
  stage_processor_t post_dump_processor, post_load_processor_t post_load_processor,
  uint32_t extended_block_count )
{
  fc::path data_dir;
  fc::path snapshot_root_dir;
  {
    ilog("Instantiating hived_fixture to generate extended block log.");
    hived_fixture fixture( true /*remove blockchain & state*/ );
    INIT_FIXTURE_2( "block-log-split", std::to_string( initial_block_storage ),
                    "plugin", "state_snapshot" );

    for( uint32_t i = 0; i < extended_block_count; ++i )
      fixture.generate_block();

    data_dir = fixture.get_data_dir();
    ilog("data_dir is ${data_dir}", (data_dir));

    snapshot_root_dir = data_dir / "snapshot_dir";
    fc::remove_all( snapshot_root_dir );

  }
  // Presumably store extended block and generate shortened state/log in this processor.
  post_generation_processor( data_dir );
  {
    ilog("Instantiating hived_fixture to dump snapshot with shortened log/state.");
    hived_fixture fixture( false /*remove blockchain & state*/ );
    fixture.postponed_init( {
      config_line( { "block-log-split", { std::to_string( initial_block_storage ) } } ),
      config_line( { "plugin", { "state_snapshot" } } ),
      config_line( { "dump-snapshot", { std::string("snap") } } ),
      config_line( { "snapshot-root-dir", { snapshot_root_dir.string() } } )
    });
  }
  // Presumably restore extended block in this processor.
  post_dump_processor( data_dir );
  {
    ilog("Instantiating hived_fixture to load snapshot with target block storage");
    hived_fixture fixture( false /*remove blockchain & state*/ );
    fixture.postponed_init( {
      config_line( { "block-log-split", { std::to_string( target_block_storage ) } } ),
      config_line( { "plugin", { "state_snapshot" } } ),
      config_line( { "load-snapshot", { std::string("snap") } } ),
      config_line( { "snapshot-root-dir", { snapshot_root_dir.string() } } )
    });
    // Presumably verify db & block storage regarding lib etc.
    post_load_processor( fixture );
  }
}

BOOST_AUTO_TEST_CASE( snapshot_replay_legacy_to_split )
{
  try {
    ilog( "Snapshot loading with incremental replay and switching block storage from legacy log file to split ones." );

    fc::path extended_log_path;
    fc::path extended_log_temp_path;

    const uint32_t short_block_count = 3*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
    const uint32_t extended_block_count = 4*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;

    // Lambda called after initial extended log was generated.
    auto post_generation_processor = [&](const fc::path& data_dir){
      fc::path block_log_dir = data_dir / "blockchain";
      extended_log_path = block_log_dir / block_log_file_name_info::_legacy_file_name;
      extended_log_temp_path = data_dir / "temp_log";
      // Hide extended log in different directory.
      fc::rename( extended_log_path, extended_log_temp_path );
      // Delete its artifacts file.
      fc::remove_all( block_log_dir / 
        (block_log_file_name_info::_legacy_file_name + block_log_file_name_info::_artifacts_extension) );

      ilog("Instantiating hived_fixture to generate 'shortened' state.");
      hived_fixture fixture( true /*remove blockchain & state*/ );
      fixture.postponed_init( {
        config_line( { "block-log-split", { std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/ ) } } ),
        config_line( { "plugin", { "state_snapshot" } } )
      });
      // Generate shortened log.
      for( uint32_t i = 0; i < short_block_count; ++i )
        fixture.generate_block();
    };

    // Lambda called after snapshot was dumped (with shortened state/log)
    auto post_dump_processor = [&](const fc::path& data_dir){
      // Restore extended log before snapshot is loaded.
      fc::rename( extended_log_temp_path, extended_log_path );
    };

    auto post_load_processor = [&](hived_fixture& fixture){
      // Verify that database was restored and incremental replay succeeded.
      auto lib_num_db = fixture.db->get_last_irreversible_block_num();
      auto lib_db = fixture.db->get_last_irreversible_block_data();
      BOOST_REQUIRE_EQUAL( extended_block_count, lib_num_db );
      BOOST_REQUIRE( lib_db );
      BOOST_REQUIRE_EQUAL( extended_block_count, lib_db->get_block_num() );

      const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
      // Verify head block from "official" irreversible block storage.
      auto lib_storage = block_reader.head_block();
      auto lib_by_number = block_reader.get_block_by_number( extended_block_count );
      BOOST_REQUIRE( lib_by_number );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_id(), lib_storage->get_block_id() );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_num(), lib_storage->get_block_num() );
      BOOST_REQUIRE_EQUAL( extended_block_count, lib_storage->get_block_num() );
    };

    // Run snapshot-load-with-incremental-replay scenario switching block storage from legacy to split.
    post_snapshot_replay_block_storage_switch( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/,
                                               9999 /*target_block_storage*/, 
                                               post_generation_processor,
                                               post_dump_processor,
                                               post_load_processor, 
                                               extended_block_count /*extended_block_count*/);
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( snapshot_replay_legacy_to_zero )
{
  try {
    ilog( "Snapshot loading with incremental replay and switching block storage from legacy log file to memory-only." );

    fc::path extended_log_path;
    fc::path extended_log_temp_path;

    const uint32_t short_block_count = 3*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
    const uint32_t extended_block_count = 4*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;

    // Lambda called after initial extended log was generated.
    auto post_generation_processor = [&](const fc::path& data_dir){
      fc::path block_log_dir = data_dir / "blockchain";
      extended_log_path = block_log_dir / block_log_file_name_info::_legacy_file_name;
      extended_log_temp_path = data_dir / "temp_log";
      // Hide extended log in different directory.
      fc::rename( extended_log_path, extended_log_temp_path );
      // Delete its artifacts file.
      fc::remove_all( block_log_dir / 
        (block_log_file_name_info::_legacy_file_name + block_log_file_name_info::_artifacts_extension) );

      ilog("Instantiating hived_fixture to generate 'shortened' state.");
      hived_fixture fixture( true /*remove blockchain & state*/ );
      fixture.postponed_init( {
        config_line( { "block-log-split", { std::to_string( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/ ) } } ),
        config_line( { "plugin", { "state_snapshot" } } )
      });
      // Generate shortened log.
      for( uint32_t i = 0; i < short_block_count; ++i )
        fixture.generate_block();
    };

    // Lambda called after snapshot was dumped (with shortened state/log)
    auto post_dump_processor = [&](const fc::path& data_dir){
      // Restore extended log before snapshot is loaded.
      fc::rename( extended_log_temp_path, extended_log_path );
    };

    auto post_load_processor = [&](hived_fixture& fixture){
      // Verify that database was restored but incremental replay was NOT run - block log is ignored.
      auto lib_num_db = fixture.db->get_last_irreversible_block_num();
      auto lib_db = fixture.db->get_last_irreversible_block_data();
      BOOST_REQUIRE_EQUAL( short_block_count, lib_num_db );
      BOOST_REQUIRE( lib_db );
      BOOST_REQUIRE_EQUAL( short_block_count, lib_db->get_block_num() );

      const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
      // Verify head block from "official" irreversible block storage.
      auto lib_storage = block_reader.head_block();
      auto lib_by_number = block_reader.get_block_by_number( short_block_count );
      BOOST_REQUIRE( lib_by_number );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_id(), lib_storage->get_block_id() );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_num(), lib_storage->get_block_num() );
      BOOST_REQUIRE_EQUAL( short_block_count, lib_storage->get_block_num() );

      // Check that there's only head block available (no tail at all).
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( short_block_count -1 ), fc::assert_exception );
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( 1 ), fc::assert_exception );
    };

    // Run snapshot-load-with-incremental-replay scenario switching block storage from legacy to memory-only.
    post_snapshot_replay_block_storage_switch( LEGACY_SINGLE_FILE_BLOCK_LOG /*initial_block_storage*/,
                                               0 /*target_block_storage*/, 
                                               post_generation_processor,
                                               post_dump_processor,
                                               post_load_processor, 
                                               extended_block_count /*extended_block_count*/);
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( snapshot_replay_split_to_split )
{
  try {
    ilog( "Verify that loading snapshot with incremental replay succeeds with split log." );

    fc::path extended_log_path;
    fc::path extended_log_temp_path;

    const uint32_t extended_block_count = 4*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;

    // Lambda called after initial extended log was generated.
    auto post_generation_processor = [&](const fc::path& data_dir){
      fc::path block_log_dir = data_dir / "blockchain";
      extended_log_path = block_log_dir / block_log_file_name_info::get_nth_part_file_name( 4 );
      extended_log_temp_path = block_log_dir / "temp_log";
      // Rename head part of the log to make it shorter.
      fc::rename( extended_log_path, extended_log_temp_path );
      // Delete its artifacts file.
      fc::remove_all( block_log_dir / 
        ( block_log_file_name_info::get_nth_part_file_name( 4 ) + block_log_file_name_info::_artifacts_extension) );

      ilog("Instantiating hived_fixture to force-replay shortened block log.");
      hived_fixture fixture( false /*remove blockchain & state*/ );
      fixture.postponed_init( {
        config_line( { "block-log-split", { std::to_string( 9999 /*initial_block_storage*/ ) } } ),
        config_line( { "plugin", { "state_snapshot" } } ),
        config_line( { "force-replay", { "" } } )
      });
    };

    auto post_dump_processor = [&](const fc::path& data_dir){
      // Restore extended head part of the log before snapshot is loaded.
      fc::rename( extended_log_temp_path, extended_log_path );
    };

    auto post_load_processor = [&](hived_fixture& fixture){
      // Verify that database was restored and incremental replay succeeded.
      auto lib_num_db = fixture.db->get_last_irreversible_block_num();
      auto lib_db = fixture.db->get_last_irreversible_block_data();
      BOOST_REQUIRE_EQUAL( extended_block_count, lib_num_db );
      BOOST_REQUIRE( lib_db );
      BOOST_REQUIRE_EQUAL( extended_block_count, lib_db->get_block_num() );

      const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
      // Verify head block from "official" irreversible block storage.
      auto lib_storage = block_reader.head_block();
      auto lib_by_number = block_reader.get_block_by_number( extended_block_count );
      BOOST_REQUIRE( lib_by_number );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_id(), lib_storage->get_block_id() );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_num(), lib_storage->get_block_num() );
      BOOST_REQUIRE_EQUAL( extended_block_count, lib_storage->get_block_num() );
    };

    // Run snapshot-load-with-incremental-replay scenario with split file block storage.
    post_snapshot_replay_block_storage_switch( 9999 /*initial_block_storage*/,
                                               9999 /*target_block_storage*/, 
                                               post_generation_processor, 
                                               post_dump_processor,
                                               post_load_processor,
                                               extended_block_count /*extended_block_count*/);
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_CASE( snapshot_replay_split_to_zero )
{
  try {
    ilog( "Verify that loading snapshot with incremental replay switching block storage from split file to memory only." );

    fc::path extended_log_path;
    fc::path extended_log_temp_path;

    const uint32_t extended_block_count = 4*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
    const uint32_t short_block_count = 3*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;

    // Lambda called after initial extended log was generated.
    auto post_generation_processor = [&](const fc::path& data_dir){
      fc::path block_log_dir = data_dir / "blockchain";
      extended_log_path = block_log_dir / block_log_file_name_info::get_nth_part_file_name( 4 );
      extended_log_temp_path = block_log_dir / "temp_log";
      // Rename head part of the log to make it shorter.
      fc::rename( extended_log_path, extended_log_temp_path );
      // Delete its artifacts file.
      fc::remove_all( block_log_dir / 
        ( block_log_file_name_info::get_nth_part_file_name( 4 ) + block_log_file_name_info::_artifacts_extension) );

      ilog("Instantiating hived_fixture to force-replay shortened block log.");
      hived_fixture fixture( false /*remove blockchain & state*/ );
      fixture.postponed_init( {
        config_line( { "block-log-split", { std::to_string( 9999 /*initial_block_storage*/ ) } } ),
        config_line( { "plugin", { "state_snapshot" } } ),
        config_line( { "force-replay", { "" } } )
      });
    };

    auto post_dump_processor = [&](const fc::path& data_dir){
      // Restore extended head part of the log before snapshot is loaded.
      fc::rename( extended_log_temp_path, extended_log_path );
    };

    auto post_load_processor = [&](hived_fixture& fixture){
      // Verify that database was restored but incremental replay was NOT run - block log is ignored.
      auto lib_num_db = fixture.db->get_last_irreversible_block_num();
      auto lib_db = fixture.db->get_last_irreversible_block_data();
      BOOST_REQUIRE_EQUAL( short_block_count, lib_num_db );
      BOOST_REQUIRE( lib_db );
      BOOST_REQUIRE_EQUAL( short_block_count, lib_db->get_block_num() );

      const block_read_i& block_reader = fixture.get_chain_plugin().block_reader();
      // Verify head block from "official" irreversible block storage.
      auto lib_storage = block_reader.head_block();
      auto lib_by_number = block_reader.get_block_by_number( short_block_count );
      BOOST_REQUIRE( lib_by_number );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_id(), lib_storage->get_block_id() );
      BOOST_REQUIRE_EQUAL( lib_by_number->get_block_num(), lib_storage->get_block_num() );
      BOOST_REQUIRE_EQUAL( short_block_count, lib_storage->get_block_num() );

      // Check that there's only head block available (no tail at all).
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( short_block_count -1 ), fc::assert_exception );
      BOOST_REQUIRE_THROW( block_reader.get_block_by_number( 1 ), fc::assert_exception );
    };

    // Run snapshot-load-with-incremental-replay scenario switching block storage from split file to memory only.
    post_snapshot_replay_block_storage_switch( 9999 /*initial_block_storage*/,
                                               0 /*target_block_storage*/, 
                                               post_generation_processor, 
                                               post_dump_processor,
                                               post_load_processor,
                                               extended_block_count /*extended_block_count*/);
  } catch (fc::exception& e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_SUITE_END()
#endif
