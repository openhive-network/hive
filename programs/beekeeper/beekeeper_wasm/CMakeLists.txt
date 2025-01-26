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

# Override common options specific to exception handling for **WHOLE SET OF HIVE SPECIFIC MODULES**
TARGET_COMPILE_OPTIONS( CommonBuildOptions INTERFACE
  -Oz
  -fwasm-exceptions
)
TARGET_LINK_OPTIONS( CommonBuildOptions INTERFACE
  -Oz
  -fwasm-exceptions
  -sEXPORT_EXCEPTION_HANDLING_HELPERS=1
  -sEXCEPTION_STACK_TRACES=1
  -sELIMINATE_DUPLICATE_FUNCTIONS=1

 )

set( SOURCES
      main.cpp
      beekeeper_wasm_app.cpp
      beekeeper_wasm_api.cpp
)

set( INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include )

function( DEFINE_BEEKEEPER_TARGET_FOR env )
      set( exec_wasm_name "beekeeper_wasm_${env}" )
      set( exec_ts_gen "beekeeper_ts_gen_${env}" )

      message(NOTICE "Configuring '${exec_wasm_name}' and '${exec_ts_gen}'")

      ADD_EXECUTABLE( ${exec_wasm_name} ${SOURCES} )

      target_include_directories( ${exec_wasm_name} PUBLIC ${INCLUDES} )

      target_link_libraries( ${exec_wasm_name} PUBLIC embind beekeeper_core )
      # add -sASSERTIONS to `target_link_options` if you want more information
      # INITIAL_MEMORY by default = 16777216
      target_link_options( ${exec_wasm_name} PUBLIC
        -sEXPORTED_RUNTIME_METHODS=["FS"] -sMODULARIZE=1 -sSINGLE_FILE=0
                                                    -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sENVIRONMENT="${env}"
                         )
      set_target_properties( ${exec_wasm_name} PROPERTIES OUTPUT_NAME "beekeeper_wasm.${env}.js" )

      # Helper target, just to perform TypeScript wrapper generation
      ADD_EXECUTABLE( ${exec_ts_gen} ${SOURCES} )

      target_include_directories( ${exec_ts_gen} PUBLIC ${INCLUDES} )
      target_link_libraries( ${exec_ts_gen} PUBLIC embind beekeeper_core )

      set_target_properties( ${exec_ts_gen} PROPERTIES OUTPUT_NAME "beekeeper_ts_gen.${env}.cjs" )
      target_link_options( ${exec_ts_gen} PUBLIC
        --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/beekeeper_wasm.${env}.d.ts" )
endfunction()

DEFINE_BEEKEEPER_TARGET_FOR(node)
DEFINE_BEEKEEPER_TARGET_FOR(web)

target_link_options( beekeeper_wasm_node PUBLIC -sUSE_ES6_IMPORT_META=1 -sNODERAWFS=1 )
target_link_libraries( beekeeper_wasm_node PUBLIC nodefs.js )
target_link_libraries( beekeeper_ts_gen_node PUBLIC nodefs.js )

target_link_options( beekeeper_wasm_web PUBLIC -sUSE_ES6_IMPORT_META=0 )
target_link_libraries( beekeeper_wasm_web PUBLIC idbfs.js )
target_link_libraries( beekeeper_ts_gen_web PUBLIC idbfs.js )
