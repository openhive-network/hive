file(GLOB HEADERS "include/hive/plugins/account_history_rocksdb/*.hpp")

add_library( account_history_rocksdb_plugin
             account_history_rocksdb_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( account_history_rocksdb_plugin
   rocksdb chain_plugin hive_chain hive_protocol json_rpc_plugin rocksdb
   )

target_include_directories( account_history_rocksdb_plugin
   PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/rocksdb/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/condenser_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/database_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/block_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/account_history_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/account_by_key_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/network_broadcast_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/p2p/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/tags_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/tags/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/follow_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/follow/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/reputation_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/reputation/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/apis/market_history_api/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/market_history/include"

   )

install( TARGETS
   account_history_rocksdb_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
