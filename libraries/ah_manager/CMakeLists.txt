file(GLOB HEADERS "include/hive/ah_manager/*.hpp")

include( GetGitRevisionDescription )
get_git_head_revision(GIT_REFSPEC HIVE_GIT_REVISION_SHA)
get_git_unix_timestamp(HIVE_GIT_REVISION_UNIX_TIMESTAMP)
git_describe(HIVE_GIT_REVISION_DESCRIPTION --tags)
if(NOT HIVE_GIT_REVISION_DESCRIPTION)
    set(HIVE_GIT_REVISION_DESCRIPTION "unknown")
endif(NOT HIVE_GIT_REVISION_DESCRIPTION)

find_package(PostgreSQL 10.0)

IF (PostgreSQL_FOUND)
  MESSAGE (STATUS "Found PostgreSQL installation.")

  FIND_LIBRARY(PQXX_LIB pqxx)
  FIND_LIBRARY(PQ_LIB pq)

  ADD_LIBRARY(  ah_manager
                utilities.cpp
                ${HEADERS} )
ELSE()
  MESSAGE (WARNING "No PostgreSQL installation found.")
ENDIF()

target_link_libraries( ah_manager hive_chain fc ${PQXX_LIB} ${PostgreSQL_LIBRARIES} )
target_include_directories( ah_manager
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                            PRIVATE "${PostgreSQL_INCLUDE_DIRS}"
                          )

if( CLANG_TIDY_EXE )
   set_target_properties(
    ah_manager PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libah_manager.control.in" "${CMAKE_CURRENT_BINARY_DIR}/libah_manager.control" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libah_manager.sql" "${CMAKE_CURRENT_BINARY_DIR}/libah_manager--${HIVE_GIT_REVISION_SHA}.sql" @ONLY)

install( TARGETS
  ah_manager

  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

