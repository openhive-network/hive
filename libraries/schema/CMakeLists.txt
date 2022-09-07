file(GLOB HEADERS "include/hive/schema/*.hpp")
add_library( hive_schema STATIC schema.cpp ${HEADERS} )
target_link_libraries( hive_schema fc )
target_include_directories( hive_schema PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   hive_schema

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/hive/schema" )
