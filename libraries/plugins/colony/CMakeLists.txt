file(GLOB HEADERS "include/hive/plugins/colony/*.hpp")

add_library( colony_plugin
             colony_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( colony_plugin p2p_plugin chain_plugin )
target_include_directories( colony_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      colony_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   colony_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
