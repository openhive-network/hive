file(GLOB HEADERS "include/hive/condenser_api_plugin/*.hpp")
add_library( condenser_api_plugin
             condenser_api.cpp
             condenser_api_plugin.cpp
             ${HEADERS} )

target_link_libraries( condenser_api_plugin
   account_by_key_api_plugin
   account_history_api_plugin
   database_api_plugin
   block_api_plugin
   reputation_api_plugin
   market_history_api_plugin
   network_broadcast_api_plugin
   hive_utilities
   rc_api_plugin )
target_include_directories( condenser_api_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      condenser_api_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   condenser_api_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/hive/condenser_api_plugin" )
