file(GLOB HEADERS "include/hive/schema/*.hpp")
add_library( hive_schema STATIC schema.cpp ${HEADERS} )
add_library( hive_schema_shared SHARED schema.cpp ${HEADERS} )
target_link_libraries( hive_schema fc )
target_link_libraries( hive_schema_shared fc_shared )
target_include_directories( hive_schema PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_include_directories( hive_schema_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   hive_schema
   hive_schema_shared

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/hive/schema" )
