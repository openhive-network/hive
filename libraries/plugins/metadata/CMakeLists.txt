file(GLOB HEADERS "include/hive/plugins/metadata/*.hpp")

add_library( metadata_plugin
             metadata_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( metadata_plugin chain_plugin )
target_include_directories( metadata_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      metadata_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   metadata_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
