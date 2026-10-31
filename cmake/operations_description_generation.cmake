# Generation of the description of operations.
#
# operations_description_generator (operations_description_generator.cpp) writes a JSON description of operations
# and the types they use, built from FC_REFLECT of operations and the C++ types of their members. The description
# does not depend on the JSON form (HF26 or legacy) - generators of API clients build both forms from it.
# The file is generated on every build of hived and contains only operations - it is not a part of openapi.json,
# generators take it as a separate input.
#
# Usage:
#   operations_description_generation( OUTPUT <path of the generated file> )

set( OPERATIONS_DESCRIPTION_GENERATOR_SOURCE "${CMAKE_CURRENT_LIST_DIR}/operations_description_generator.cpp" )

function( operations_description_generation )
  cmake_parse_arguments( OPERATIONS_DESCRIPTION "" "OUTPUT" "" ${ARGN} )
  if( NOT OPERATIONS_DESCRIPTION_OUTPUT )
    message( FATAL_ERROR "operations_description_generation: OUTPUT is required" )
  endif()

  add_executable( operations_description_generator "${OPERATIONS_DESCRIPTION_GENERATOR_SOURCE}" )
  # libraries of hive_protocol are listed explicitly before OpenSSL - static libraries of fc need it
  find_package( OpenSSL REQUIRED )
  target_link_libraries( operations_description_generator
    PRIVATE hive_protocol hive_schema_core fc_core fc OpenSSL::SSL OpenSSL::Crypto ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

  get_filename_component( OUTPUT_DIRECTORY "${OPERATIONS_DESCRIPTION_OUTPUT}" DIRECTORY )
  file( MAKE_DIRECTORY "${OUTPUT_DIRECTORY}" )
  add_custom_command(
    OUTPUT "${OPERATIONS_DESCRIPTION_OUTPUT}"
    COMMAND operations_description_generator "${OPERATIONS_DESCRIPTION_OUTPUT}"
    DEPENDS operations_description_generator
    COMMENT "Generating description of operations: ${OPERATIONS_DESCRIPTION_OUTPUT}"
  )
  add_custom_target( operations_description ALL DEPENDS "${OPERATIONS_DESCRIPTION_OUTPUT}" )
endfunction()
