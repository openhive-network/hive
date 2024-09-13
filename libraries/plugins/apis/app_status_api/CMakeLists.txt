file(GLOB HEADERS "include/hive/plugins/app_status_api/*.hpp")
add_library( app_status_api_plugin
             app_status_api.cpp
             app_status_api_plugin.cpp
             ${HEADERS} )

target_link_libraries( app_status_api_plugin json_rpc_plugin appbase )
target_include_directories( app_status_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      app_status_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   app_status_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/hive/app_status_api" )
