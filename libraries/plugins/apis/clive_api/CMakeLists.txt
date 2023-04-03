file(GLOB HEADERS "include/hive/plugins/clive_api/*.hpp")
add_library( clive_api_plugin
             clive_api_plugin.cpp
             clive_api.cpp
             ${HEADERS}
           )

target_link_libraries( clive_api_plugin clive_plugin json_rpc_plugin )
target_include_directories( clive_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

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
