file(GLOB HEADERS "include/equihash/*.hpp" )

SET( sources src/pow.cpp src/blake2b.c ${HEADERS} )

add_library( equihash ${sources})
add_library( equihash_shared SHARED ${sources})

target_link_libraries( equihash )
target_link_libraries( equihash_shared )
target_include_directories( equihash PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_include_directories( equihash_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
set_target_properties( equihash_shared PROPERTIES CXX_VISIBILITY_PRESET "default" )

install( TARGETS
   equihash equihash_shared

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
