
PROJECT( BeekeeperWASM )

CMAKE_MINIMUM_REQUIRED( VERSION 3.22.1 )

SET(CMAKE_CXX_STANDARD 17)

ADD_SUBDIRECTORY( beekeeper_wasm_core )

ADD_EXECUTABLE( beekeeper_wasm main.cpp )

TARGET_LINK_LIBRARIES( beekeeper_wasm PRIVATE beekeeper_wasm_core )