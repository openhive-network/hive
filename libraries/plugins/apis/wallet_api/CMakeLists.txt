file(GLOB HEADERS "include/hive/plugins/wallet_api/*.hpp")
add_library( wallet_api_plugin
             wallet_api_plugin.cpp
             wallet_api.cpp
             ${HEADERS}
           )

target_link_libraries( wallet_api_plugin wallet_plugin json_rpc_plugin )
target_include_directories( wallet_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      wallet_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   wallet_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
