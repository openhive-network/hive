SET( schema_dir "../schema/")

file(GLOB HEADERS "${schema_dir}/include/hive/schema/*.hpp")
add_library( hive_schema_minimal STATIC ${schema_dir}/schema.cpp ${HEADERS} )
target_include_directories( hive_schema_minimal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${schema_dir}/include" )
target_link_libraries( hive_schema_minimal PUBLIC fc_minimal )
