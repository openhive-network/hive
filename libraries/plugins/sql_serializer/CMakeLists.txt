file(GLOB HEADERS "include/hive/plugins/sql_serializer/*.hpp")

add_library( sql_serializer
             sql_serializer.cpp
             ${HEADERS}
           )

target_link_libraries( sql_serializer chain_plugin hive_chain hive_protocol )
target_include_directories( sql_serializer
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      sql_serializer PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   sql_serializer

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
