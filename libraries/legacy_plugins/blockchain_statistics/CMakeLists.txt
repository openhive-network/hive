file(GLOB HEADERS "include/hive/blockchain_statistics/*.hpp")

add_library( hive_blockchain_statistics
             blockchain_statistics_plugin.cpp
             blockchain_statistics_api.cpp
           )

target_link_libraries( hive_blockchain_statistics hive_chain hive_protocol hive_app )
target_include_directories( hive_blockchain_statistics
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   hive_blockchain_statistics

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)