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

file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper_wasm/*.hpp")

include( ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/hive_targets.cmake )

set(BOOST_COMPONENTS)
LIST(APPEND BOOST_COMPONENTS
      atomic
      chrono
      date_time
      filesystem
      program_options
      system)

set(HIVE_BUILD_ON_MINIMAL_FC ON)

add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../core beekeeper_core )
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../libraries/fc build_fc_minimal )
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../libraries/schema build_schema_minimal )
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../libraries/protocol build_protocol_minimal )

set( SOURCES
      main.cpp
      beekeeper_wasm_app.cpp
      beekeeper_wasm_api.cpp
)

ADD_EXECUTABLE( beekeeper_wasm ${SOURCES} )

set( INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include )

target_include_directories( beekeeper_wasm PUBLIC ${INCLUDES} )
target_compile_options( beekeeper_wasm PUBLIC -fexceptions )
target_link_libraries( beekeeper_wasm PUBLIC embind hive_protocol beekeeper_core idbfs.js )
# add -sASSERTIONS to `target_link_options` if you want more information
# INITIAL_MEMORY by default = 16777216
target_link_options( beekeeper_wasm PUBLIC -fexceptions -sMODULARIZE=1 -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 )
set_target_properties( beekeeper_wasm PROPERTIES OUTPUT_NAME beekeeper_wasm.mjs )

# Helper target, just to perform TypeScript wrapper generation
ADD_EXECUTABLE( beekeeper_ts_gen ${SOURCES} )

target_include_directories( beekeeper_ts_gen PUBLIC ${INCLUDES} )
target_compile_options( beekeeper_ts_gen PUBLIC -fexceptions )
target_link_libraries( beekeeper_ts_gen PUBLIC embind hive_protocol beekeeper_core idbfs.js )

target_link_options( beekeeper_ts_gen PUBLIC -fexceptions --embind-emit-tsd ${CMAKE_CURRENT_BINARY_DIR}/beekeeper.d.ts )
set_target_properties( beekeeper_ts_gen PROPERTIES OUTPUT_NAME beekeeper_ts_gen.cjs )
