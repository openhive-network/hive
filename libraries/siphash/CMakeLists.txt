file(GLOB HEADERS "include/hive/siphash/*.hpp")

# header-only, so the hashers of hashed indexes keep inlining the hash
add_library( hive_siphash INTERFACE )
target_include_directories( hive_siphash INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( FILES ${HEADERS} DESTINATION "include/hive/siphash" )
