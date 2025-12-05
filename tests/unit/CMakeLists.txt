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

function(parse_unit_tests RESULT)
   set(SOURCES)
	foreach(_element ${ARGN})
      list(APPEND SOURCES "${_element}")
	endforeach()

   set(tests)

   foreach(src ${SOURCES})
      file(READ ${src} thefile)
      #string(REGEX MATCH "" test_suite "${thefile}" )
      string(REGEX MATCH "BOOST_FIXTURE_TEST_SUITE\\([A-Za-z0-9_, ]*\\)" test_suite "${thefile}" )

      if( NOT (test_suite STREQUAL "") )
         string(SUBSTRING "${test_suite}" 25 -1 test_suite)
         string(FIND "${test_suite}" "," comma_loc )
         string(SUBSTRING "${test_suite}" 0 ${comma_loc} test_suite)
         string(STRIP "${test_suite}" test_suite)

         string( REGEX MATCHALL "BOOST_AUTO_TEST_CASE\\([A-Za-z0-9_, ]*\\)" cases "${thefile}" )

         foreach( test_case ${cases} )
            string(SUBSTRING "${test_case}" 22 -1 test_case)
            string(FIND "${test_case}" ")" paren_loc )
            string(SUBSTRING "${test_case}" 0 ${paren_loc} test_case)
            string(STRIP "${test_case}" test_case)

            list(APPEND tests "${test_suite}/${test_case}")
         endforeach()
      endif()
   endforeach()

   set(${RESULT} ${tests} PARENT_SCOPE)
endfunction()

parse_unit_tests(TEST_CASES ${UNIT_TESTS})

#message(STATUS "${TEST_CASES}")

# Build chain_test executable (tests discovered dynamically via --list_content)
include_directories(${Boost_INCLUDE_DIRS})
add_executable(chain_test ${UNIT_TESTS})
set_target_properties(chain_test PROPERTIES ENABLE_EXPORTS true)
target_link_libraries(chain_test
   db_fixture chainbase hive_chain hive_protocol
   account_history_rocksdb_plugin market_history_plugin witness_plugin debug_node_plugin
   fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${PLATFORM_SPECIFIC_LIBS})

# Run chain_test suites in parallel using GNU parallel
add_test(NAME unit/chain_test
   COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_chain_tests_parallel.sh $<TARGET_FILE:chain_test>
   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

file(GLOB PLUGIN_TESTS
      "plugin_tests/*.cpp"
)

parse_unit_tests(TEST_CASES ${PLUGIN_TESTS})

#message(STATUS "${TEST_CASES}")

add_boost_test( plugin_test
   SOURCES ${PLUGIN_TESTS}
   TESTS
    json_rpc/basic_validation
    json_rpc/syntax_validation
    json_rpc/misc_validation
    json_rpc/positive_validation
    json_rpc/semantics_validation
    market_history/mh_test:142
    transaction_status/transaction_status_test
    metadata_api_tests/get_account_metadata_test
    metadata_api_tests/find_account_metadata_test
    metadata_api_tests/account_create_metadata_test
)

# needed to correctly print crash stacktrace
set_target_properties(plugin_test PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( plugin_test db_fixture hive_chain hive_protocol account_history_rocksdb_plugin market_history_plugin witness_plugin debug_node_plugin condenser_api_plugin database_api_plugin transaction_status_plugin transaction_status_api_plugin metadata_plugin metadata_api_plugin fc ${PLATFORM_SPECIFIC_LIBS} block_log_info_plugin hive_plugins )

if(MSVC)
  set_source_files_properties( tests/serialization_tests.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

#add_subdirectory( generate_empty_blocks )
