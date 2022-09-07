file(GLOB HEADERS "include/equihash/*.hpp" )

SET( sources src/pow.cpp src/blake2b.c ${HEADERS} )

add_library( equihash ${sources})

target_include_directories( equihash PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   equihash

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
