set(CHAIN_SOURCES "")
get_target_property( CHAIN_SOURCES hive_chain SOURCES )
#message("chain library sources: ${CHAIN_SOURCES}")
list( TRANSFORM CHAIN_SOURCES PREPEND "../libraries/chain/" )
#message("chain library sources: ${CHAIN_SOURCES}")

## Section dedicated to generation & verification of unique ids of assertions in protocol sources.
set(ASSERTION_ID_GENERATOR_SOURCE ${GENERATED_ASSERT_ID_DIR}/chain_assertion_id_generator.cpp)
set(ASSERTION_ID_VERIFICATION_HEADER_SOURCE ${GENERATED_ASSERT_ID_DIR}/chain_assertion_hashes_verifier.hpp)
set(ASSERTION_ID_WAX_INLINE_SOURCE ${GENERATED_ASSERT_ID_DIR}/chain_assertion_hashes_wax.inl)
set(ASSERTION_ID_VERIFIER_SOURCE ${GENERATED_ASSERT_ID_DIR}/chain_assertion_id_verifier.cpp)
# 1. Use separate cmake script to parse all assertions and dump their info into generated cpp file.
include("../cmake/assert_id_generation.cmake")
assert_id_generation(
  sources "${CHAIN_SOURCES}"
  generator_output_path "${ASSERTION_ID_GENERATOR_SOURCE}"
  verifier_header_path "${ASSERTION_ID_VERIFICATION_HEADER_SOURCE}"
  wax_inline_path "${ASSERTION_ID_WAX_INLINE_SOURCE}"
  verifier_output_path "${ASSERTION_ID_VERIFIER_SOURCE}"
  namespace "chain"
)
# 2. Build executable, linking it with fc to enable unique id generation for assertions.
#    Run the generator post build to dump a header file with ids.
add_executable( chain_assertion_id_generator ${ASSERTION_ID_GENERATOR_SOURCE})
target_link_libraries( chain_assertion_id_generator PUBLIC fc )
add_custom_command(
  OUTPUT ${ASSERTION_ID_VERIFICATION_HEADER_SOURCE} ${ASSERTION_ID_WAX_INLINE_SOURCE}
  COMMAND chain_assertion_id_generator
  DEPENDS chain_assertion_id_generator
)
# 3. Build the verifier to verify uniqueness of the hashes dumped in previous step.
SET( CHAIN_ASSERTION_ID_VERIFIER_SOURCES
  ${ASSERTION_ID_VERIFIER_SOURCE}
  ${ASSERTION_ID_VERIFICATION_HEADER_SOURCE}
)
add_executable( chain_assertion_id_verifier ${CHAIN_ASSERTION_ID_VERIFIER_SOURCES} )
add_dependencies( chain_assertion_id_verifier chain_assertion_id_generator )
##


# Generate verifier source, including all headers generated earlier by submodules.
set(VERIFIER_LINES)
file(GLOB ASSERT_ID_HEADERS "${GENERATED_ASSERT_ID_DIR}/*.hpp")
foreach(ASSERT_ID_HEADER_PATH ${ASSERT_ID_HEADERS})
  list(APPEND VERIFIER_LINES "#include \"${ASSERT_ID_HEADER_PATH}\"\n")
endforeach()
list(APPEND VERIFIER_LINES "int main( int argc, char** argv ){\n")
list(APPEND VERIFIER_LINES "  return 0\;\n")
list(APPEND VERIFIER_LINES "}\n")
# Dumps source only when different than previous version.
set(VERIFIER_SOURCE_FILE ${GENERATED_ASSERT_ID_DIR}/assertion_id_verifier.cpp)
set(TEMP_FILE "${VERIFIER_SOURCE_FILE}_tmp")
file(WRITE ${TEMP_FILE} ${VERIFIER_LINES} )
file(COPY_FILE ${TEMP_FILE} ${VERIFIER_SOURCE_FILE} ONLY_IF_DIFFERENT)
# Build the verifier to verify uniqueness of the hashes dumped in previous step.
set( ASSERTION_ID_VERIFIER_SOURCES
  ${VERIFIER_SOURCE_FILE}
  ${ASSERT_ID_HEADERS}
)
add_executable( assertion_id_verifier ${ASSERTION_ID_VERIFIER_SOURCES} )
target_include_directories( assertion_id_verifier PUBLIC "${GENERATED_ASSERT_ID_DIR}" )
# Add below other submodule's needing assertion id verification.
add_dependencies( assertion_id_verifier protocol_assertion_id_generator )
add_dependencies( assertion_id_verifier chain_assertion_id_generator )
