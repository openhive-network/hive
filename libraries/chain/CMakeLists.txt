file(GLOB HEADERS "include/hive/chain/*.hpp" "include/hive/chain/util/*.hpp" "include/hive/chain/smt_objects/*.hpp" "include/hive/chain/sps_objects/*.hpp")

if (NOT BUILD_HIVE_TESTNET)
  if( IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/compression_dictionaries"
      AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/compression_dictionaries/CMakeLists.txt" )
    add_subdirectory(compression_dictionaries)
    set( COMPRESSION_DICTIONARY_LIBRARIES hive_chain_compression_dictionaries )
    set( HAS_COMPRESSION_DICTIONARIES TRUE )
  else()
    message( FATAL_ERROR  "The compression_dicitonaries submodule has not been cloned, it is required for mainnet builds")
  endif()
else()
  set( COMPRESSION_DICTIONARY_LIBRARIES )
  set( HAS_COMPRESSION_DICTIONARIES FALSE )
endif()

## SORT .cpp by most likely to change / break compile
add_library( hive_chain

             # As database takes the longest to compile, start it first
             database.cpp
             index.cpp
             index-01.cpp
             index-02.cpp
             index-03.cpp
             index-04.cpp
             index-05.cpp
             index-06.cpp
             index-07.cpp
             index-08.cpp
             index-09.cpp
             index-10.cpp
             index-11.cpp

             full_block.cpp
             full_transaction.cpp
             blockchain_worker_thread_pool.cpp

             smt_evaluator.cpp
             sps_evaluator.cpp

             smt_objects/smt_market_maker.cpp
             smt_objects/nai_pool.cpp
             hive_evaluator.cpp
             hive_objects.cpp
             required_action_evaluator.cpp
             optional_action_evaluator.cpp

             witness_schedule.cpp
             fork_database.cpp

             shared_authority.cpp
             block_log.cpp
             block_compression_dictionaries.cpp

             generic_custom_operation_interpreter.cpp

             util/rd.cpp
             util/reward.cpp
             util/extractors.cpp
             util/advanced_benchmark_dumper.cpp
             util/smt_token.cpp
             util/sps_processor.cpp
             util/sps_helper.cpp
             util/delayed_voting.cpp
             util/owner_update_limit_mgr.cpp
             util/operation_extractor.cpp
             util/data_filter.cpp

             ${HEADERS}
           )

target_link_libraries( hive_chain hive_jsonball hive_protocol fc chainbase hive_schema appbase libzstd_static ${COMPRESSION_DICTIONARY_LIBRARIES}
                       ${PATCH_MERGE_LIB} ${BROTLI_LIBRARIES} ${ZLIB_LIBRARIES})
target_include_directories( hive_chain
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${BROTLI_INCLUDE_DIRS}" "${ZLIB_INCLUDE_DIRS}"
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/zstd/lib")
if ( HAS_COMPRESSION_DICTIONARIES )
  target_compile_definitions( hive_chain PUBLIC -DHAS_COMPRESSION_DICTIONARIES )
endif()

if( CLANG_TIDY_EXE )
   set_target_properties(
      hive_chain PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

if(MSVC)
  set_source_files_properties( database.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

INSTALL( TARGETS
   hive_chain

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/hive/chain" )
