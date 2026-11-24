file(GLOB HEADERS "include/hive/schema/*.hpp")
add_library( hive_schema_core ${HIVE_LIBRARY_TYPE} schema.cpp ${HEADERS} )
target_include_directories( hive_schema_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_link_libraries( hive_schema_core PUBLIC ${HIVE_FC_CORE_LIBRARY} )

add_library( hive_schema INTERFACE )
target_link_libraries( hive_schema INTERFACE hive_schema_core fc )

add_library( hive_schema_shared_boost INTERFACE )

target_link_libraries( hive_schema_shared_boost INTERFACE hive_schema_core fc_shared_boost )

install( TARGETS
   hive_schema

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

install( TARGETS
   hive_schema_shared_boost

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

install( FILES ${HEADERS} DESTINATION "include/hive/schema" )
