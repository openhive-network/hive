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
set(VERIFIER_SOURCE_FILE ${GENERATED_ASSERT_ID_DIR}/assert_id_verifier.cpp)
set(TEMP_FILE "${VERIFIER_SOURCE_FILE}_tmp")
file(WRITE ${TEMP_FILE} ${VERIFIER_LINES} )
file(COPY_FILE ${TEMP_FILE} ${VERIFIER_SOURCE_FILE} ONLY_IF_DIFFERENT)
# Build the verifier to verify uniqueness of the hashes dumped in previous step.
set( ASSERTION_ID_VERIFIER_SOURCES
  ${VERIFIER_SOURCE_FILE}
  ${ASSERT_ID_HEADERS}
)
add_executable( assertion_id_verifier ${ASSERTION_ID_VERIFIER_SOURCES} )
# Add below other submodule's needing assertion id verification.
add_dependencies( assertion_id_verifier protocol_assertion_id_generator )
