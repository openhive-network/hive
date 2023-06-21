
PROJECT( BeekeeperWASM )

CMAKE_MINIMUM_REQUIRED( VERSION 3.22.1 )

SET(CMAKE_CXX_STANDARD 17)

SET( WASM_COMPILE_MODE ON )

ADD_SUBDIRECTORY( beekeeper_wasm_core )

ADD_EXECUTABLE( beekeeper_wasm main.cpp )

if( CMAKE_TOOLCHAIN_FILE )
  MESSAGE( STATUS "WASM compiler is enabled" )
  TARGET_LINK_LIBRARIES( beekeeper_wasm PRIVATE beekeeper_wasm_core embind idbfs.js )
  TARGET_LINK_OPTIONS( beekeeper_wasm PRIVATE "--emrun" )
else()
  MESSAGE( STATUS "WASM compiler is disabled" )
  TARGET_LINK_LIBRARIES( beekeeper_wasm PRIVATE beekeeper_wasm_core )
endif()