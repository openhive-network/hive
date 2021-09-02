file(GLOB HEADERS "include/hive/plugins/sql_serializer/*.hpp")

find_package(PostgreSQL 10.0)

FIND_LIBRARY(PQXX_LIB pqxx)
FIND_LIBRARY(PQ_LIB pq)

IF (PostgreSQL_FOUND AND PQXX_LIB AND PQ_LIB)
  MESSAGE (STATUS "Found PostgreSQL installation.")

  ADD_LIBRARY( sql_serializer_plugin
               sql_serializer.cpp
               data_processor.cpp
               transaction_controllers.cpp
               ${HEADERS}
              )
ELSE()
  MESSAGE (WARNING "No PostgreSQL installation found. sql_serializer plugin functionality WILL BE UNAVAILABLE. Check installation of ubuntu package: postgresql-server-dev-all")
  ADD_LIBRARY( sql_serializer_plugin
               fake_sql_serializer.cpp
               ${HEADERS}
              )
  set (PQXX_LIB "")
  set (PostgreSQL_LIBRARIES "")
ENDIF()

target_link_libraries( sql_serializer_plugin chain_plugin hive_chain hive_protocol  ${PQXX_LIB} ${PostgreSQL_LIBRARIES} )

target_include_directories( sql_serializer_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                            PRIVATE "${PostgreSQL_INCLUDE_DIRS}"
                          )

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
