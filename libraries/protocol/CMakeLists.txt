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

SET( STATIC_SOURCES authority_verification_tracer.cpp
   testnet_blockchain_configuration.cpp
   smt_operations.cpp
   hive_operations.cpp
   hive_custom_operations.cpp
   dhf_operations.cpp
   types.cpp
   schema_types.cpp
   authority.cpp
   operations.cpp
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
)

## Section dedicated to generation & verification of unique ids of assertions in protocol sources.
set(ASSERTION_ID_GENERATOR_SOURCE ${GENERATED_ASSERT_ID_DIR}/protocol_assertion_id_generator.cpp)
set(ASSERTION_ID_VERIFICATION_HEADER_SOURCE ${GENERATED_ASSERT_ID_DIR}/protocol_assertion_hashes_verifier.hpp)
set(ASSERTION_ID_WAX_INLINE_SOURCE ${GENERATED_ASSERT_ID_DIR}/protocol_assertion_hashes_wax.inl)
set(ASSERTION_ID_VERIFIER_SOURCE ${GENERATED_ASSERT_ID_DIR}/protocol_assertion_id_verifier.cpp)
# 1. Use separate cmake script to parse all assertions and dump their info into generated cpp file.
assert_id_generation(
  "${STATIC_SOURCES}"
  "${ASSERTION_ID_GENERATOR_SOURCE}"
  "${ASSERTION_ID_VERIFICATION_HEADER_SOURCE}"
  "${ASSERTION_ID_WAX_INLINE_SOURCE}"
  "${ASSERTION_ID_VERIFIER_SOURCE}"
)
# 2. Build executable, linking it with fc to enable unique id generation for assertions.
#    Run the generator post build to dump a header file with ids.
add_executable( protocol_assertion_id_generator ${ASSERTION_ID_GENERATOR_SOURCE})
target_link_libraries( protocol_assertion_id_generator PUBLIC fc )
add_custom_command(
  OUTPUT ${ASSERTION_ID_VERIFICATION_HEADER_SOURCE} ${ASSERTION_ID_WAX_INLINE_SOURCE}
  COMMAND protocol_assertion_id_generator
  DEPENDS protocol_assertion_id_generator
)
# 3. Build the verifier to verify uniqueness of the hashes dumped in previous step.
SET( ASSERTION_ID_VERIFIER_SOURCES
  ${ASSERTION_ID_VERIFIER_SOURCE}
  ${ASSERTION_ID_VERIFICATION_HEADER_SOURCE}
)
add_executable( protocol_assertion_id_verifier ${ASSERTION_ID_VERIFIER_SOURCES} )
add_dependencies( protocol_assertion_id_verifier protocol_assertion_id_generator )
##

SET( SOURCES
   ${STATIC_SOURCES}
   ${hardfork_hpp_file}
   "${BINARY_INCLUDE_DIR}/hardfork.hpp"
   "${BINARY_INCLUDE_DIR}/words.deflate" "${BINARY_INCLUDE_DIR}/words.b64"
)

# #embed directive start supported in Clang-19
if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION STRGREATER_EQUAL "19.0.0" )
  set(CMAKE_C_STANDARD 23)
  set(C23_EMBED_SUPPORTED 1)
endif()

cmake_policy(SET CMP0041 NEW)

## SORT .cpp by most likely to change / break compile
add_library( hive_protocol STATIC ${SOURCES} )

file(MAKE_DIRECTORY ${BINARY_INCLUDE_DIR})

add_custom_command(
  COMMAND "bash" "-c" "\"${CMAKE_CURRENT_SOURCE_DIR}/generate_words.sh\" \"${CMAKE_CURRENT_SOURCE_DIR}/words.txt\" \"${BINARY_INCLUDE_DIR}/words.deflate\" \"${BINARY_INCLUDE_DIR}/words.b64\""
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/words.txt"
  OUTPUT "${BINARY_INCLUDE_DIR}/words.deflate" "${BINARY_INCLUDE_DIR}/words.b64"
  VERBATIM
)

if( C23_EMBED_SUPPORTED )
  target_compile_definitions(hive_protocol PRIVATE C23_EMBED_SUPPORTED)
  target_compile_options(hive_protocol PRIVATE "--embed-dir=${BINARY_INCLUDE_DIR}")
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
