set(HIVE_BUILD_ON_MINIMAL_FC ON)
set(BOOST_COMPONENTS)
LIST(APPEND BOOST_COMPONENTS
  chrono
  date_time
  filesystem
  program_options
  system)
include( ${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/hive_targets.cmake )
add_subdirectory( ${CMAKE_CURRENT_LIST_DIR}/../../../../libraries/fc build_fc_minimal )

set(WASM_RUNTIME_COMPONENT_NAME "wasm_runtime_components")

function( DEFINE_WASM_TARGET_FOR wasm_target_basename )
  set(options WASM_USE_FS)
  set(oneValueArgs TARGET_ENVIRONMENT)
  set(multiValueArgs LINK_LIBRARIES LINK_OPTIONS)
  cmake_parse_arguments(PARSE_ARGV 0 arg
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
  )

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

  set( exec_wasm_name "${wasm_target_basename}.${arg_TARGET_ENVIRONMENT}" )
  set( exec_common_name "${wasm_target_basename}.common" )

  message(NOTICE "Configuring '${exec_wasm_name}'")

  IF ("${arg_TARGET_ENVIRONMENT}" STREQUAL "web")
    MESSAGE( STATUS "Chosen web target environment")
    SET ( NODE_ENV 0 )
    SET ( WASM_ENV "web" )
  ELSE()
    MESSAGE( STATUS "Chosen node target environment")
    SET ( NODE_ENV 1 )
    SET ( WASM_ENV "node" )
  ENDIF()

  ADD_EXECUTABLE( ${exec_wasm_name} ${SOURCES} )

  target_include_directories( ${exec_wasm_name} PUBLIC ${INCLUDES} )

  target_link_libraries( ${exec_wasm_name} PUBLIC embind )
  # add -sASSERTIONS to `target_link_options` if you want more information
  # INITIAL_MEMORY by default = 16777216
  target_link_options( ${exec_wasm_name} PUBLIC
    -sMODULARIZE=1 -sSINGLE_FILE=0 -sUSE_ES6_IMPORT_META=1
    -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sWASM_ASYNC_COMPILATION=1
    --minify=0 --emit-symbol-map -sENVIRONMENT=${WASM_ENV}
    --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
  )

  if (arg_LINK_OPTIONS)
    target_link_options( ${exec_wasm_name} PUBLIC ${arg_LINK_OPTIONS})
  endif()

  if (arg_LINK_LIBRARIES)
    target_link_libraries( ${exec_wasm_name} PUBLIC ${arg_LINK_LIBRARIES})
  endif()

  set_target_properties( ${exec_wasm_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}"
    OUTPUT_NAME "${exec_common_name}.js"
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}/${exec_common_name}.js"
    RENAME "${exec_wasm_name}.js"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}/${exec_common_name}.wasm"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES  "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )
endfunction()
