file(GLOB HEADERS "include/hive/plugins/comment_cashout_logging/*.hpp")

add_library( comment_cashout_logging_plugin
             comment_cashout_logging_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( comment_cashout_logging_plugin chain_plugin hive_chain hive_protocol )
target_include_directories( comment_cashout_logging_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      comment_cashout_logging_plugin PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   comment_cashout_logging_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
