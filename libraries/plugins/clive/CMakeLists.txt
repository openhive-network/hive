file(GLOB HEADERS "include/hive/plugins/clive/*.hpp")

add_library( clive_plugin
             clive.cpp
             clive_plugin.cpp
             wallet_manager.cpp
             ${HEADERS} )

target_link_libraries( clive_plugin appbase )
target_include_directories( clive_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      clive_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   clive_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

