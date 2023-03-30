file(GLOB HEADERS "include/hive/plugins/wallet/*.hpp")

add_library( wallet_plugin
             wallet.cpp
             wallet_plugin.cpp
             wallet_manager.cpp
             ${HEADERS} )

target_link_libraries( wallet_plugin appbase )
target_include_directories( wallet_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      wallet_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   wallet_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

