add_subdirectory(db_fixture)
add_subdirectory(bmic_objects)

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

INCLUDE(CTest)

ENABLE_TESTING()

file(GLOB UNIT_TESTS
   "tests/*.cpp"
)

include(BoostTestTargets.cmake)
include(BoostTestDiscoverTests.cmake)

add_boost_test( chain_test
   SOURCES ${UNIT_TESTS}
)

# needed to correctly print crash stacktrace
set_target_properties(chain_test PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( chain_test db_fixture chainbase hive_chain hive_protocol account_history_rocksdb_plugin market_history_plugin witness_plugin debug_node_plugin fc ${PLATFORM_SPECIFIC_LIBS} )

boosttest_discover_tests(chain_test
   TEST_PREFIX unit/chain_test-
   TEST_NAME_SEPARATOR /
   DISCOVERY_TIMEOUT 60
)

# Tests with explicit COST for CTest scheduling (higher cost = starts first)
# Properties are set in chain_test_properties.cmake after test discovery
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/chain_test_properties.cmake")

file(GLOB PLUGIN_TESTS
      "plugin_tests/*.cpp"
)

add_boost_test( plugin_test
   SOURCES ${PLUGIN_TESTS}
)

# needed to correctly print crash stacktrace
set_target_properties(plugin_test PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( plugin_test db_fixture hive_chain hive_protocol account_history_rocksdb_plugin market_history_plugin witness_plugin debug_node_plugin condenser_api_plugin database_api_plugin transaction_status_plugin transaction_status_api_plugin fc ${PLATFORM_SPECIFIC_LIBS} block_log_info_plugin hive_plugins )

boosttest_discover_tests(plugin_test
   TEST_PREFIX unit/plugin_test-
   TEST_NAME_SEPARATOR /
)

# Tests with explicit COST for CTest scheduling (higher cost = starts first)
# Properties are set in plugin_test_properties.cmake after test discovery
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/plugin_test_properties.cmake")

if(MSVC)
  set_source_files_properties( tests/serialization_tests.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

#add_subdirectory( generate_empty_blocks )
