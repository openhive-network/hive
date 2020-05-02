file(GLOB HEADERS "include/hive/plugins/auth_util/*.hpp")

add_library( hive_auth_util
             ${HEADERS}
             auth_util_plugin.cpp
             auth_util_api.cpp
           )

target_link_libraries( hive_auth_util hive_app hive_chain hive_protocol fc )
target_include_directories( hive_auth_util
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
