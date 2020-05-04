file(GLOB HEADERS "include/hive/private_message/*.hpp")

add_library( hive_private_message
             private_message_plugin.cpp
           )

target_link_libraries( hive_private_message hive_chain hive_protocol hive_app )
target_include_directories( hive_private_message
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   hive_private_message

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
