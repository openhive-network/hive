file(GLOB HEADERS "include/hive/plugins/metadata_api/*.hpp")
add_library( metadata_api_plugin
             metadata_api_plugin.cpp
             metadata_api.cpp
             ${HEADERS}
           )

target_link_libraries( metadata_api_plugin metadata_plugin json_rpc_plugin )
target_include_directories( metadata_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      metadata_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   metadata_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
