PROJECT( BeekeeperWASM )
cmake_minimum_required( VERSION 3.22.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )
set( CMAKE_EXECUTABLE_SUFFIX "" )

SET( CMAKE_VERBOSE_MAKEFILE ON )

cmake_policy(SET CMP0057 NEW)

file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper_wasm/*.hpp")

include( ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/hive_targets.cmake )

set(BOOST_COMPONENTS)
LIST(APPEND BOOST_COMPONENTS
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

function( DEFINE_BEEKEEPER_TARGET_FOR )
      set( exec_wasm_name "beekeeper.common" )
      set( exec_ts_gen "beekeeper_ts_gen_common" )

      message(NOTICE "Configuring '${exec_wasm_name}' and '${exec_ts_gen}' in ${TARGET_ENVIRONMENT} environment")

      IF ("${TARGET_ENVIRONMENT}" STREQUAL "web")
        MESSAGE( STATUS "Chosen web target environment")
        SET ( NODE_ENV 0 )
      ELSE()
        MESSAGE( STATUS "Chosen node target environment")
        SET ( NODE_ENV 1 )
      ENDIF()

      ADD_EXECUTABLE( ${exec_wasm_name} ${SOURCES} )

      target_include_directories( ${exec_wasm_name} PUBLIC ${INCLUDES} )

      target_link_libraries( ${exec_wasm_name} PUBLIC embind beekeeper_core )
      # add -sASSERTIONS to `target_link_options` if you want more information
      # INITIAL_MEMORY by default = 16777216
      target_link_options( ${exec_wasm_name} PUBLIC
        -sEXPORTED_RUNTIME_METHODS=["FS"] -sMODULARIZE=1 -sSINGLE_FILE=0
        -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sWASM_ASYNC_COMPILATION=1
        --minify=0
        --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/${exec_wasm_name}.d.ts"
       )

       set_target_properties( ${exec_wasm_name} PROPERTIES OUTPUT_NAME "${exec_wasm_name}.js" )

       IF(${NODE_ENV})
         target_link_options( ${exec_wasm_name} PUBLIC -sENVIRONMENT="node" -sNODERAWFS=1)
         target_link_libraries( ${exec_wasm_name} PUBLIC nodefs.js )
       ELSE()
         target_link_options( ${exec_wasm_name} PUBLIC -sENVIRONMENT="web")
         target_link_libraries( ${exec_wasm_name} PUBLIC idbfs.js)
       endif()

      INSTALL( FILES "$<TARGET_FILE_DIR:${exec_wasm_name}>/${exec_wasm_name}.js"
        RENAME "beekeeper_wasm.${TARGET_ENVIRONMENT}.js"
        COMPONENT "${exec_wasm_name}_runtime"
        DESTINATION .
        )

      INSTALL( FILES "$<TARGET_FILE_DIR:${exec_wasm_name}>/${exec_wasm_name}.wasm"
        COMPONENT "${exec_wasm_name}_runtime"
        DESTINATION .
        )

      INSTALL( FILES  "${CMAKE_CURRENT_BINARY_DIR}/${exec_wasm_name}.d.ts"
        COMPONENT "${exec_wasm_name}_runtime"
        DESTINATION .
        )
endfunction()

DEFINE_BEEKEEPER_TARGET_FOR()
