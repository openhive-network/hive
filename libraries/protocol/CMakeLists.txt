file(GLOB HEADERS "include/hive/protocol/*.hpp")
file(GLOB HF_PARTS "hardfork.d/*.hf")

set(SOURCE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/hive/protocol)
set(BINARY_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol)

if( MSVC )
   set( hardfork_hpp_file "${SOURCE_INCLUDE_DIR}/hardfork.hpp" )
   add_custom_target( build_hardfork_hpp
      COMMAND cat-parts "${CMAKE_CURRENT_SOURCE_DIR}/hardfork.d" ${hardfork_hpp_file} )
   add_dependencies( build_hardfork_hpp cat-parts )
else( MSVC )
   set( hardfork_hpp_file "${BINARY_INCLUDE_DIR}/hardfork.hpp" )
   add_custom_command(
      COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../../programs/build_helpers
      python3 -m hive_build_helpers.cat_parts
      "${CMAKE_CURRENT_SOURCE_DIR}/hardfork.d"
      ${hardfork_hpp_file}
      OUTPUT "${BINARY_INCLUDE_DIR}/hardfork.hpp"
      DEPENDS ${HF_PARTS}
   )
endif( MSVC )

include("${CMAKE_CURRENT_SOURCE_DIR}/get_config.d/generate_get_config.cmake")
generate_get_config(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/hive/protocol/config.hpp"  # path to config.hpp
  "${CMAKE_CURRENT_SOURCE_DIR}/get_config.d/get_config.cpp.in"    # path to get_config template file
  "${CMAKE_CURRENT_BINARY_DIR}/get_config.cpp"                    # output path
)

#############

SET( SOURCES testnet_blockchain_configuration.cpp
   smt_operations.cpp
   hive_operations.cpp
   hive_custom_operations.cpp
   dhf_operations.cpp
   types.cpp
   schema_types.cpp
   authority.cpp
   operations.cpp
   sign_state.cpp
   transaction.cpp
   transaction_util.cpp
   block.cpp
   asset.cpp
   version.cpp
   key_utils.cpp
   misc_utilities.cpp
   forward_impacted.cpp
   forward_keyauths.cpp
   fixed_string.cpp
   crypto_memo.cpp
   words.cpp
   words.c
   ${GET_CONFIG_CPP}
   ${HEADERS}
   ${hardfork_hpp_file}
   "${BINARY_INCLUDE_DIR}/hardfork.hpp"
)

# #embed directive start supported in Clang-19
if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION STRGREATER_EQUAL "19.0.0" )
  set(CMAKE_C_STANDARD 23)
  set(C23_EMBED_SUPPORTED 1)
else()
  list(APPEND SOURCES "${BINARY_INCLUDE_DIR}/words.b64")
endif()

cmake_policy(SET CMP0041 NEW)

## SORT .cpp by most likely to change / break compile
add_library( hive_protocol STATIC ${SOURCES} )

file(MAKE_DIRECTORY ${BINARY_INCLUDE_DIR})
execute_process(COMMAND "zip" "-9" "-D" "-j" "-v" "${BINARY_INCLUDE_DIR}/words.zip" "words.txt"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMAND_ERROR_IS_FATAL ANY)

if( C23_EMBED_SUPPORTED )
  target_compile_definitions(hive_protocol PRIVATE C23_EMBED_SUPPORTED)
  target_compile_options(hive_protocol PRIVATE "--embed-dir=${BINARY_INCLUDE_DIR}")
else()
  execute_process(COMMAND "base64" "-w 0" "words.zip"
                  WORKING_DIRECTORY ${BINARY_INCLUDE_DIR}
                  OUTPUT_VARIABLE B64_CONTENT
                  COMMAND_ERROR_IS_FATAL ANY)
  file(WRITE ${BINARY_INCLUDE_DIR}/words.b64 "\"")
  file(APPEND ${BINARY_INCLUDE_DIR}/words.b64 ${B64_CONTENT})
  file(APPEND ${BINARY_INCLUDE_DIR}/words.b64 "\"")
endif()

target_link_libraries( hive_protocol PUBLIC hive_schema_core fc_core )

target_link_libraries ( hive_protocol PUBLIC
   $<IF:$<OR:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>>, hive_schema_shared_boost, hive_schema>
)

target_link_libraries ( hive_protocol PUBLIC
$<IF:$<OR:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>>, fc_shared_boost, fc>
)

target_include_directories( hive_protocol
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties( hive_protocol PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
endif( CLANG_TIDY_EXE )

INSTALL( TARGETS
   hive_protocol

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

INSTALL( FILES ${HEADERS} DESTINATION "include/hive/protocol" )
