file(GLOB HEADERS "include/hive/plugins/queen/*.hpp")

add_library( queen_plugin
             queen_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( queen_plugin witness_plugin chain_plugin )
target_include_directories( queen_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      queen_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   queen_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
