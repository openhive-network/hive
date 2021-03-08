file(GLOB HEADERS "include/hive/plugins/network_node_api_plugin/*.hpp")
add_library( network_node_api_plugin
             network_node_api.cpp
             network_node_api_plugin.cpp
             ${HEADERS} )

target_link_libraries( network_node_api_plugin chain_plugin json_rpc_plugin p2p_plugin appbase )
target_include_directories( network_node_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      network_node_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   network_node_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/hive/network_node_api_plugin" )
