macro(generate_get_config path_to_config_hpp get_config_cpp_in get_config_cpp_out)

  file(COPY ${path_to_config_hpp} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  set(path_to_new_config_hpp "${CMAKE_CURRENT_BINARY_DIR}/config.hpp")

  # removing includes and pragmas
  file(STRINGS ${path_to_new_config_hpp} file_content )
  list(FILTER file_content EXCLUDE REGEX "#(include|pragma)" )
  set(prepared_get_content)
  foreach(x ${file_content})
    list( APPEND prepared_get_content "${x}\n" )
  endforeach()

  set(_OUT_FILE "${path_to_new_config_hpp}.pregen.preprocessed.hpp" )
  set(OUT_FILE "${path_to_new_config_hpp}.preprocessed" )

  # rewriting gile
  file(WRITE ${_OUT_FILE} ${prepared_get_content} )

  # setup compiler flags for dry run
  set(local_compiler_flags "-E")

  IF( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
    list(APPEND local_compiler_flags "-fdirectives-only" "${_OUT_FILE}" "-o" "${OUT_FILE}")
  ELSE()
    list(APPEND local_compiler_flags "-dD" "${_OUT_FILE}" "-o" "${OUT_FILE}")
  ENDIF()
  
  if( BUILD_HIVE_TESTNET  )
    list(APPEND local_compiler_flags "-DIS_TEST_NET")
  endif()
  if( ENABLE_SMT_SUPPORT  )
    list(APPEND local_compiler_flags "-DHIVE_ENABLE_SMT")
  endif()

  # using of compiler dry run to preprocess config.hpp
  message("running c++ compiler wit flags: ${local_compiler_flags}")
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} ${local_compiler_flags})

  # trim additional output from comiler till the comment from header file
  file(STRINGS ${OUT_FILE} config_hpp_content )
  set(preamula_on 1)
  set(list_of_new_lines)
  foreach(line in ${config_hpp_content})
    if( ${preamula_on} EQUAL 1 )
      string(FIND "${line}" "Steemit" steemit_found )
      if( ${steemit_found} GREATER_EQUAL 0 )
        set(preamula_on 0)
      else()
        continue()
      endif()
    endif()

    # parse defines
    if(${line} MATCHES "^([ ]*#define)")
      string(REGEX REPLACE "#define ([A-Z0-9_]+) .*" "\\1" UNSAFE_VALUE "${line}")
      string(STRIP ${UNSAFE_VALUE} VALUE)
      if( NOT ${VALUE} STREQUAL "HIVE_CHAIN_ID" )
        list( APPEND list_of_new_lines "  result(\"${VALUE}\", ${VALUE})\;\n" )
      endif()
    endif()
  endforeach()

  # convert list to single varriable
  set(CONFIG_HPP)
  foreach(x ${list_of_new_lines})
    set(CONFIG_HPP "${CONFIG_HPP}${x}")
  endforeach()

  configure_file(${get_config_cpp_in} ${get_config_cpp_out} )
  message("get_config.cpp has been generated in `${get_config_cpp_out}`")
  set(GET_CONFIG_CPP ${get_config_cpp_out})

endmacro()