PROJECT( BeekeeperWASM )
cmake_minimum_required( VERSION 3.22.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )
set( CMAKE_EXECUTABLE_SUFFIX "" )

#SET( CMAKE_VERBOSE_MAKEFILE ON )

cmake_policy(SET CMP0057 NEW)

ADD_SUBDIRECTORY( beekeeper_wasm_core )

ADD_EXECUTABLE( beekeeper_wasm main.cpp )

target_compile_options( beekeeper_wasm PUBLIC -fexceptions )
target_link_libraries( beekeeper_wasm PUBLIC embind beekeeper_wasm_core idbfs.js )
# -sFORCE_FILESYSTEM=1 -sMODULARIZE=1 -sFILESYSTEM=1 
target_link_options( beekeeper_wasm PUBLIC -fexceptions -sMODULARIZE=1 -sEXPORT_ES6=1 )
set_target_properties( beekeeper_wasm PROPERTIES OUTPUT_NAME beekeeper_wasm.mjs )

# Helper target, just to perform TypeScript wrapper generation
ADD_EXECUTABLE( beekeeper_ts_gen main.cpp )

target_compile_options( beekeeper_ts_gen PUBLIC -fexceptions )
target_link_libraries( beekeeper_ts_gen PUBLIC embind beekeeper_wasm_core idbfs.js )

target_link_options( beekeeper_ts_gen PUBLIC -fexceptions --embind-emit-tsd ${CMAKE_CURRENT_BINARY_DIR}/beekeeper.d.ts )
set_target_properties( beekeeper_ts_gen PROPERTIES OUTPUT_NAME beekeeper_ts_gen.js )
