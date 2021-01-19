file(GLOB HEADERS "include/hive/plugins/wallet_bridge_api/*.hpp")

add_library( wallet_bridge_api_plugin
             wallet_bridge_api.cpp
             wallet_bridge_api_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( wallet_bridge_api_plugin
   account_by_key_api_plugin
   account_history_api_plugin
   block_api_plugin
   chain_plugin
   database_api_plugin
   market_history_api_plugin
   network_broadcast_api_plugin
   json_rpc_plugin )
target_include_directories( wallet_bridge_api_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      wallet_bridge_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   wallet_bridge_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
