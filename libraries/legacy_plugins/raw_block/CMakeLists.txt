file(GLOB HEADERS "include/hive/plugins/raw_block/*.hpp")

add_library( hive_raw_block
             ${HEADERS}
             raw_block_plugin.cpp
             raw_block_api.cpp
           )

target_link_libraries( hive_raw_block hive_app hive_chain hive_protocol fc )
target_include_directories( hive_raw_block
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
