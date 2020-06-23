file(GLOB HEADERS "include/hive/plugins/block_info/*.hpp")

add_library( hive_block_info
             ${HEADERS}
             block_info_plugin.cpp
             block_info_api.cpp
           )

target_link_libraries( hive_block_info hive_app hive_chain hive_protocol fc )
target_include_directories( hive_block_info
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
