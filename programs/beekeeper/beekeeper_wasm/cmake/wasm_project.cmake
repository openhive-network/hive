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

function( DEFINE_WASM_TARGET_FOR wasm_target_basename )
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

  set( exec_wasm_name "${wasm_target_basename}.common" )

  message(NOTICE "Configuring '${exec_wasm_name}' in ${TARGET_ENVIRONMENT} environment")

  IF ("${TARGET_ENVIRONMENT}" STREQUAL "web")
    MESSAGE( STATUS "Chosen web target environment")
    SET ( NODE_ENV 0 )
    SET ( WASM_ENV "web" )
  ELSE()
    MESSAGE( STATUS "Chosen node target environment")
    SET ( NODE_ENV 1 )
    SET ( WASM_ENV "node" )
  ENDIF()

  IF (${WASM_USE_FS})
    MESSAGE( STATUS "using filesystem support")
    SET ( WASM_USE_FS 1 )
  ENDIF()

  ADD_EXECUTABLE( ${exec_wasm_name} ${SOURCES} )

  target_include_directories( ${exec_wasm_name} PUBLIC ${INCLUDES} )

  target_link_libraries( ${exec_wasm_name} PUBLIC embind )
  # add -sASSERTIONS to `target_link_options` if you want more information
  # INITIAL_MEMORY by default = 16777216
  target_link_options( ${exec_wasm_name} PUBLIC
    -sEXPORTED_RUNTIME_METHODS=["FS"]
    -sMODULARIZE=1 -sSINGLE_FILE=0 -sUSE_ES6_IMPORT_META=1
    -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sWASM_ASYNC_COMPILATION=1
    --minify=0 --emit-symbol-map -sENVIRONMENT="${WASM_ENV}"
    --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/${exec_wasm_name}.d.ts"
  )

  IF (${WASM_USE_FS})
    target_link_options( ${exec_wasm_name} PUBLIC -sEXPORTED_RUNTIME_METHODS=["FS"] )

    IF(${NODE_ENV})
      target_link_options( ${exec_wasm_name} PUBLIC -sNODERAWFS=1)
      target_link_libraries( ${exec_wasm_name} PUBLIC nodefs.js )
    ELSE()
      target_link_libraries( ${exec_wasm_name} PUBLIC idbfs.js)
    endif()
  endif()

  set_target_properties( ${exec_wasm_name} PROPERTIES OUTPUT_NAME "${exec_wasm_name}.js" )

  INSTALL( FILES "$<TARGET_FILE_DIR:${exec_wasm_name}>/${exec_wasm_name}.js"
    RENAME "${wasm_target_basename}.${TARGET_ENVIRONMENT}.js"
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
