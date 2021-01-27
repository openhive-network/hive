file(GLOB HEADERS "include/hive/plugins/sql_serializer/*.hpp")

set(PQXX /usr/local/include/pqxx)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)

add_library( sql_serializer_plugin
             sql_serializer.cpp
             data_processor.cpp
             ${HEADERS}
            )

target_link_libraries( sql_serializer_plugin chain_plugin hive_chain hive_protocol condenser_api_plugin ${PQXX_LIB} ${PQ_LIB} )
target_include_directories( sql_serializer_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
		sql_serializer_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
sql_serializer_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
