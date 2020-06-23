file(GLOB HEADERS "include/hive/account_statistics/*.hpp")

add_library( hive_account_statistics
             account_statistics_plugin.cpp
             account_statistics_api.cpp
           )

target_link_libraries( hive_account_statistics hive_chain hive_protocol hive_app )
target_include_directories( hive_account_statistics
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   hive_account_statistics

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)