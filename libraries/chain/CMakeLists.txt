file(GLOB HEADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "include/hive/chain/*.hpp" "include/hive/chain/util/*.hpp" "include/hive/chain/smt_objects/*.hpp" "include/hive/chain/dhf_objects/*.hpp")

if( NOT DEFINED SKIP_COMPRESSION_DICTIONARIES )
  if( IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/compression_dictionaries"
      AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/compression_dictionaries/CMakeLists.txt" )
    add_subdirectory(compression_dictionaries)
    set( COMPRESSION_DICTIONARY_LIBRARIES hive_chain_compression_dictionaries )
    set( HAS_COMPRESSION_DICTIONARIES TRUE )
  else()
    message( FATAL_ERROR  "The compression_dicitonaries submodule has not been cloned, it is required for builds")
  endif()
endif()

## SORT .cpp by most likely to change / break compile
set( SOURCES
             # Database split into multiple files for faster parallel compilation
             database.cpp
             database_init.cpp
             database_hardfork.cpp
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
             index-12.cpp
             index-13.cpp

             full_block.cpp
             full_transaction.cpp
             blockchain_worker_thread_pool.cpp

             smt_evaluator.cpp
             dhf_evaluator.cpp

             smt_objects/smt_market_maker.cpp
             smt_objects/nai_pool.cpp
             # hive_evaluator split into multiple files for faster parallel compilation
             hive_evaluator.cpp
             hive_evaluator_transfer.cpp
             hive_evaluator_account.cpp
             hive_evaluator_social.cpp
             hive_objects.cpp

             witness_schedule.cpp
             fork_database.cpp

             shared_authority.cpp
             block_compression_dictionaries.cpp
             block_flow_control.cpp
             transaction_flow_control.cpp
             block_log.cpp
             block_log_artifacts.cpp
             block_log_compression.cpp
             block_log_wrapper.cpp
             block_storage_interface.cpp
             fork_db_block_reader.cpp
             irreversible_block_data.cpp
             irreversible_block_writer.cpp
             single_block_storage.cpp
             sync_block_writer.cpp
             split_block_log.cpp

             generic_custom_operation_interpreter.cpp

             util/rd.cpp
             util/reward.cpp
             util/extractors.cpp
             util/advanced_benchmark_dumper.cpp
             util/smt_token.cpp
             util/decoded_types_data_storage.cpp
             util/dhf_processor.cpp
             util/dhf_helper.cpp
             util/delayed_voting.cpp
             util/owner_update_limit_mgr.cpp
             util/operation_extractor.cpp
             util/data_filter.cpp
             util/state_checker_tools.cpp

             rc/rc_curve.cpp
             rc/rc_objects.cpp
             rc/rc_utility.cpp
             rc/resource_count.cpp
             rc/resource_sizes.cpp
             rc/resource_user.cpp

             external_storage/memory_comment_archive.cpp
             external_storage/placeholder_comment_archive.cpp
             external_storage/rocksdb_storage_provider.cpp
             external_storage/rocksdb_comment_storage_provider.cpp
             external_storage/rocksdb_comment_archive.cpp
             external_storage/rocksdb_snapshot.cpp
             external_storage/types.cpp

             ${HEADERS}
           )


add_library( hive_chain
  ${SOURCES}
)

target_link_libraries( hive_chain rocksdb hive_jsonball hive_protocol fc chainbase hive_schema appbase libzstd_static ${COMPRESSION_DICTIONARY_LIBRARIES}
                       ${PATCH_MERGE_LIB} ${BROTLI_LIBRARIES} ${ZLIB_LIBRARIES})
target_include_directories( hive_chain
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${BROTLI_INCLUDE_DIRS}" "${ZLIB_INCLUDE_DIRS}"
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/zstd/lib")
if ( HAS_COMPRESSION_DICTIONARIES )
  target_compile_definitions( hive_chain PUBLIC -DHAS_COMPRESSION_DICTIONARIES )
endif()

# Disable implicit template instantiation to speed up compilation
# Templates will only be instantiated where explicitly used
#if( NOT MSVC )
#  target_compile_options( hive_chain PRIVATE -fno-implicit-templates )
#endif()

# Precompiled header for heavy Boost headers
# Note: chainbase.hpp is excluded because it has complex dependencies on fc types
# that require specific header ordering for boost::hash specializations
target_precompile_headers( hive_chain
  PRIVATE
    <fc/crypto/ripemd160.hpp>
    <boost/multi_index_container.hpp>
    <boost/multi_index/member.hpp>
    <boost/multi_index/composite_key.hpp>
    <boost/multi_index/mem_fun.hpp>
    <boost/multi_index/tag.hpp>
    <boost/multi_index/ordered_index.hpp>
    <boost/multi_index/hashed_index.hpp>
    <boost/interprocess/managed_mapped_file.hpp>
    <boost/interprocess/containers/map.hpp>
    <boost/interprocess/containers/set.hpp>
    <boost/interprocess/containers/deque.hpp>
    <boost/interprocess/containers/string.hpp>
    <boost/interprocess/allocators/allocator.hpp>
    <chainbase/chainbase.hpp>
)

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
