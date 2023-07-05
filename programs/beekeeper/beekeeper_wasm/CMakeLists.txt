PROJECT( BeekeeperWASM )
cmake_minimum_required( VERSION 3.22.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

cmake_policy(SET CMP0057 NEW)

ADD_SUBDIRECTORY( beekeeper_wasm_core )

ADD_EXECUTABLE( beekeeper_wasm beekeeper_wasm_api.cpp )

TARGET_LINK_LIBRARIES( beekeeper_wasm PRIVATE beekeeper_wasm_core )