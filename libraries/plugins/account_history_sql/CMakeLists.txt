file(GLOB HEADERS "include/hive/plugins/account_history_sql/*.hpp")

set(PQXX /usr/local/include/pqxx)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)

add_library( account_history_sql_plugin
             account_history_sql.cpp
             ${HEADERS}
           )

target_link_libraries( account_history_sql_plugin chain_plugin hive_chain hive_protocol ${PQXX_LIB} ${PQ_LIB} )
target_include_directories( account_history_sql_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
		account_history_sql_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
account_history_sql_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
