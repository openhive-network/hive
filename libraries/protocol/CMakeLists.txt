file(GLOB HEADERS "include/hive/protocol/*.hpp")
file(GLOB HF_PARTS "hardfork.d/*.hpp")

if( MSVC )
   set( hardfork_hpp_file "${CMAKE_CURRENT_SOURCE_DIR}/include/hive/protocol/hardfork.hpp" )
   add_custom_target( build_hardfork_hpp
      COMMAND cat-parts "${CMAKE_CURRENT_SOURCE_DIR}/hardfork.d" ${hardfork_hpp_file} )
   add_dependencies( build_hardfork_hpp cat-parts )
else( MSVC )
   set( hardfork_hpp_file "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp" )
   add_custom_command(
      COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../../programs/build_helpers
      python3 -m hive_build_helpers.cat_parts
      "${CMAKE_CURRENT_SOURCE_DIR}/hardfork.d"
      ${hardfork_hpp_file}
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
      DEPENDS ${HF_PARTS}
   )
endif( MSVC )

############ generating get_config.cpp

set( path_to_config_hpp "${CMAKE_CURRENT_SOURCE_DIR}/include/hive/protocol/config.hpp" )
set( get_config_cpp_in "${CMAKE_CURRENT_SOURCE_DIR}/get_config.d/get_config.cpp.in" )

set(GET_CONFIG_CPP ${CMAKE_CURRENT_BINARY_DIR}/get_config.cpp)

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
  list(APPEND local_compiler_flags "-fdirectives-only" "${_OUT_FILE}" "-o" "${OUT_FILE}")
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
    if(${line} MATCHES "^(#define)")
      string(REGEX REPLACE "#define ([A-Z0-9_]+) .*" "\\1" VALUE "${line}")
      list( APPEND list_of_new_lines "  result[\"${VALUE}\"] = ${VALUE}\;\n" )
    endif()
  endforeach()

  # convert list to single varriable
  set(CONFIG_HPP)
  foreach(x ${list_of_new_lines})
    set(CONFIG_HPP "${CONFIG_HPP}${x}")
  endforeach()

  configure_file(${get_config_cpp_in} ${GET_CONFIG_CPP} )
  message("get_config.cpp has been generated in `${GET_CONFIG_CPP}`")

#############

## SORT .cpp by most likely to change / break compile
add_library( hive_protocol

             testnet_blockchain_configuration.cpp
             smt_operations.cpp
             hive_operations.cpp
             sps_operations.cpp
             hive_required_actions.cpp
             hive_optional_actions.cpp
             types.cpp
             schema_types.cpp
             authority.cpp
             operations.cpp
             sign_state.cpp
             transaction.cpp
             block.cpp
             asset.cpp
             version.cpp
             ${GET_CONFIG_CPP}

             ${HEADERS}
             ${hardfork_hpp_file}
             "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
           )

target_link_libraries( hive_protocol hive_schema fc )
target_include_directories( hive_protocol
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      hive_protocol PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

INSTALL( TARGETS
   hive_protocol

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

INSTALL( FILES ${HEADERS} DESTINATION "include/hive/protocol" )
