file(GLOB HEADERS "include/hive/plugins/pacemaker/*.hpp")

add_library( pacemaker_plugin
             pacemaker_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( pacemaker_plugin p2p_plugin chain_plugin hive_protocol appbase hive_chain hive_utilities )
target_include_directories( pacemaker_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      pacemaker_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   pacemaker_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
