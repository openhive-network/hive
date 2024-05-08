add_executable( get_dev_key get_dev_key.cpp )

target_link_libraries( get_dev_key
                       PRIVATE hive_chain hive_protocol hive_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   get_dev_key

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)


add_executable( test_shared_mem test_shared_mem.cpp )

target_link_libraries( test_shared_mem
                       PRIVATE  hive_chain hive_protocol hive_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

add_executable( sign_digest sign_digest.cpp )

target_link_libraries( sign_digest
                       PRIVATE hive_chain hive_protocol hive_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   sign_digest

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( sign_transaction sign_transaction.cpp )

# needed to correctly print crash stacktrace
set_target_properties(sign_transaction PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( sign_transaction
                       PRIVATE hive_chain hive_protocol hive_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   sign_transaction

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( serialize_set_properties serialize_set_properties.cpp )

target_link_libraries( serialize_set_properties
                       PRIVATE hive_chain hive_protocol hive_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   serialize_set_properties

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( inflation_model inflation_model.cpp )
target_link_libraries( inflation_model
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   inflation_model

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( dump_hive_schema dump_hive_schema.cpp )
target_link_libraries( dump_hive_schema
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
install( TARGETS
   dump_hive_schema

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( schema_test schema_test.cpp )
target_link_libraries( schema_test
                       PRIVATE hive_chain fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   schema_test

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( test_block_log test_block_log.cpp )
target_link_libraries( test_block_log
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIB} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   test_block_log

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( truncate_block_log truncate_block_log.cpp )
target_link_libraries( truncate_block_log
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIB} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   truncate_block_log

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( test_fixed_string test_fixed_string.cpp )
target_link_libraries( test_fixed_string
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIB} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   test_fixed_string

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( test_sqrt test_sqrt.cpp )
target_link_libraries( test_sqrt PRIVATE fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
install( TARGETS
   test_sqrt

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( block_log_util block_log_util.cpp )
target_link_libraries( block_log_util PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
install( TARGETS
   block_log_util

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( compress_block_log compress_block_log.cpp )

# needed to correctly print crash stacktrace
set_target_properties(compress_block_log PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( compress_block_log
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIB} ${PLATFORM_SPECIFIC_LIBS})

install( TARGETS
   compress_block_log

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( block_log_test block_log_test.cpp )
target_link_libraries( block_log_test
                       PRIVATE hive_chain hive_protocol fc ${CMAKE_DL_LIB} ${PLATFORM_SPECIFIC_LIBS})

install( TARGETS
   compress_block_log

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( decoded_types_data_storage_tester decoded_types_data_storage_tester.cpp )
target_link_libraries( decoded_types_data_storage_tester
                       PRIVATE hive_chain hive_protocol hive_plugins fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
install( TARGETS
   decoded_types_data_storage_tester

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( shared_memory_file_util shared_memory_file_util.cpp )
target_link_libraries( shared_memory_file_util
                       PRIVATE hive_chain hive_plugins fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
install( TARGETS
   shared_memory_file_util

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

set( bls_dependencies "/home/mario/src/a.237.chia-networks-bls/bls-signatures/build/src" )
set( bls_dependencies2 "/home/mario/src/a.237.chia-networks-bls/bls-signatures/build/_deps/sodium-build" )
set( bls_include "/home/mario/src/a.237.chia-networks-bls/bls-signatures/src" )
set( bls_include2 "/home/mario/src/a.237.chia-networks-bls/bls-signatures/build/_deps/blst-src" )

add_executable( bls_test bls_test.cpp )
target_link_libraries( bls_test ${bls_dependencies}/libbls.a )
target_link_libraries( bls_test fc ${bls_dependencies2}/libsodium.a )
target_include_directories( bls_test PUBLIC ${bls_include} ${bls_include2} )

install( TARGETS
   bls_test

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

add_executable( verify_authority verify_authority.cpp )

# needed to correctly print crash stacktrace
set_target_properties(verify_authority PROPERTIES ENABLE_EXPORTS true)

target_link_libraries( verify_authority
                       PRIVATE hive_protocol fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   verify_authority

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
