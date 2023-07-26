file( GLOB HEADERS "include/hive/plugins/clive_api/*.hpp" )
set( SOURCES clive_api_plugin.cpp clive_api.cpp )

add_library( clive_api_plugin ${SOURCES} ${HEADERS} )

target_include_directories( clive_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_link_libraries( clive_api_plugin PUBLIC json_rpc_plugin account_history_api_plugin database_api_plugin rc_api_plugin reputation_api_plugin )


if( CLANG_TIDY_EXE )
   set_target_properties(
      clive_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   clive_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
