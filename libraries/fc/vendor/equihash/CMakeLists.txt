file(GLOB HEADERS "include/equihash/*.hpp" )

add_library( equihash
             src/pow.cpp
             src/blake2b.c
             ${HEADERS}
           )

target_link_libraries( equihash )
target_include_directories( equihash PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   equihash

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
