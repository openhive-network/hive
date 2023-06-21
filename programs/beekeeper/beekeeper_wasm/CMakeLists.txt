
SET( WASM_COMPILE_MODE ON )

ADD_SUBDIRECTORY( beekeeper_wasm_core )

ADD_EXECUTABLE( beekeeper_wasm main.cpp )

TARGET_LINK_LIBRARIES( beekeeper_wasm PUBLIC beekeeper_wasm_core )