# Store the path to the extraction script at the time this file is processed
# This ensures the path is relative to this file's location, not the calling file's location
set(ASSERTION_ID_EXTRACTOR_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/extract_assertion_ids.py")

macro(assert_id_generation)
  set(multiValueArgs sources)
  set(option "")
  set(oneValueArgs
    generator_output_path
    verifier_header_path
    wax_inline_path
    verifier_output_path
    namespace)
  cmake_parse_arguments( assertion_id "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  message("Beginning verification of assertion's given id in ${assertion_id_namespace} namespace")

  set(assert_expressions)

  if(assertion_id_sources)
    if(NOT Python3_EXECUTABLE)
      find_package(Python3 COMPONENTS Interpreter REQUIRED)
    endif()

    set(assertion_id_extractor "${ASSERTION_ID_EXTRACTOR_SCRIPT}")
    set(assertion_id_sources_list "${CMAKE_CURRENT_BINARY_DIR}/assertion_id_sources.txt")
    file(WRITE "${assertion_id_sources_list}" "")
    foreach(file ${assertion_id_sources})
      file(REAL_PATH "${file}" real_path)
      file(APPEND "${assertion_id_sources_list}" "${real_path}\n")
    endforeach()

    set(temp_generator "${assertion_id_generator_output_path}_tmp")
    message("Script will write into temporary file ${temp_generator}")

    message("Running python to generate assertion ids")
    execute_process(
      COMMAND ${Python3_EXECUTABLE} "${assertion_id_extractor}"
        "${assertion_id_sources_list}"
        "${assertion_id_namespace}"
        "${assertion_id_verifier_header_path}"
        "${assertion_id_wax_inline_path}"
        "${temp_generator}"
      OUTPUT_VARIABLE assertion_id_generator_output
      ERROR_VARIABLE assertion_id_generator_error
      RESULT_VARIABLE assertion_id_generator_result
    )

    if(NOT assertion_id_generator_result EQUAL 0)
      message(FATAL_ERROR "Failed to extract assertion ids:\n${assertion_id_generator_error}")
    endif()

    if(NOT EXISTS "${temp_generator}")
      message(FATAL_ERROR "${temp_generator} was not created by the extraction script.")
    endif()

    file(REMOVE "${assertion_id_sources_list}")
  endif()


file(COPY_FILE ${temp_generator} ${assertion_id_generator_output_path} ONLY_IF_DIFFERENT)

set(assert_id_verifier)
list(APPEND assert_id_verifier
"#include \"${assertion_id_verifier_header_path}\"\n;"
"int main( int argc, char** argv ){\n;"
"  return 0\;\n;"
"}\n")
set(temp_verifier "${assertion_id_verifier_output_path}_tmp")
file(WRITE ${temp_verifier} ${assert_id_verifier} )
file(COPY_FILE ${temp_verifier} ${assertion_id_verifier_output_path} ONLY_IF_DIFFERENT)

endmacro()
