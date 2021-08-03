file(GLOB HEADERS "include/hive/sql_utilities/*.hpp")

find_package(PostgreSQL 10.0)
FIND_LIBRARY(PQXX_LIB pqxx)
FIND_LIBRARY(PQ_LIB pq)

IF (PostgreSQL_FOUND AND PQXX_LIB AND PQ_LIB)
  MESSAGE (STATUS "Found PostgreSQL installation.")

  ADD_LIBRARY(  sql_hive_utilities
                transaction_controllers.cpp
                ${HEADERS} )
ELSE()
  MESSAGE (WARNING "No PostgreSQL installation found.")
ENDIF()

target_link_libraries( sql_hive_utilities fc ${PQXX_LIB} ${PostgreSQL_LIBRARIES} )
target_include_directories( sql_hive_utilities
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                            PRIVATE "${PostgreSQL_INCLUDE_DIRS}"
                          )

if( CLANG_TIDY_EXE )
   set_target_properties(
      sql_hive_utilities PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
  sql_hive_utilities

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

