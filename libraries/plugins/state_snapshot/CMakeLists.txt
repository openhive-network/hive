file(GLOB HEADERS "include/hive/plugins/state_snapshot/*.hpp")

add_library( state_snapshot_plugin
             state_snapshot_plugin.cpp
             state_snapshot_tools.cpp
             state_snapshot_manifest.cpp
             ${HEADERS}
           )

target_link_libraries( state_snapshot_plugin
   rocksdb chain_plugin hive_chain hive_protocol json_rpc_plugin
   )

target_include_directories( state_snapshot_plugin
   PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/rocksdb/include"
   )

install( TARGETS
   state_snapshot_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
