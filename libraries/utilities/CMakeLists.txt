list( APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/libraries/fc/GitVersionGen" )
include( GetGitRevisionDescription )
get_git_head_revision(GIT_REFSPEC HIVE_GIT_REVISION_SHA)
get_git_unix_timestamp(HIVE_GIT_REVISION_UNIX_TIMESTAMP)
git_describe(HIVE_GIT_REVISION_DESCRIPTION --tags)
if(NOT HIVE_GIT_REVISION_DESCRIPTION)
    set(HIVE_GIT_REVISION_DESCRIPTION "unknown")
endif(NOT HIVE_GIT_REVISION_DESCRIPTION)

file(GLOB HEADERS "include/hive/utilities/*.hpp")

set(sources
   notifications.cpp
   benchmark_dumper.cpp
   key_conversion.cpp
   string_escape.cpp
   tempdir.cpp
   words.cpp
   logging_config.cpp
   database_configuration.cpp
   ${HEADERS})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/git_revision.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp" @ONLY)
list(APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp")

add_library( hive_utilities
             ${sources}
             ${HEADERS} )
target_link_libraries( hive_utilities fc hive_protocol )
target_include_directories( hive_utilities
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
if (USE_PCH)
  set_target_properties(hive_utilities PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(hive_utilities)
endif(USE_PCH)

if( CLANG_TIDY_EXE )
   set_target_properties(
      hive_utilities PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   hive_utilities

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/hive/utilities" )
