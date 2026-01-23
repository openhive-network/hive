# Copyright 2020, 2021 Deniz Bahadir and contributors
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# This code is based on an implementation of the `gtest_discover_tests()`
# functionality that was originally distributed together with CMake unter the
# OSI-approved BSD 3-Clause License. The original authors and contributors of
# that code were so kind to agree to dual-license their work also under the
# Boost Software License, Version 1.0, so that this derived worked can be
# distributed under the same license as well.
# A public record of their granted permission can be found in this discussion
# of merge-request 4145 in the CMake issue tracker:
#   https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4145#note_998500
#
# Therefore many thanks go to the following original authors and contributors:
#   Matthew Woehlke <matthew.woehlke@kitware.com>
#   Steffen Seckler <steffen.seckler@smartronic.de>
#   Ryan Thornton   <ThorntonRyan@JohnDeere.com>
#   Kevin Puetz     <PuetzKevinA@JohnDeere.com>
#   Stefan Floeren  <stefan.floeren@smartronic.de>
#   Alexander Stein <alexander.stein@mailbox.org>
#
# Deniz Bahadir in August of 2021.
#

cmake_minimum_required(VERSION ${CMAKE_VERSION})

# Overwrite possibly existing ${__CTEST_FILE} with empty file.
set(flush_tests_MODE WRITE)


# Flushes script to ${__CTEST_FILE}.
macro(flush_script)
  file(${flush_tests_MODE} "${__CTEST_FILE}" "${script}")
  set(flush_tests_MODE APPEND)
  set(script "")
endmacro()


# Flushes tests_buffer to tests variable.
macro(flush_tests_buffer)
  list(APPEND tests "${tests_buffer}")
  set(tests_buffer "")
endmacro()


# Removes surrounding double quotes (if any) from value of given variable VAR.
function(remove_outer_quotes VAR)
  string(REGEX REPLACE "^\"(.*)\"$" "\\1" ${VAR} "${${VAR}}")
  set(${VAR} "${${VAR}}" PARENT_SCOPE)
endfunction()


# Adds commands to script.
macro(add_command NAME)
  set(_args "")
  foreach(_arg ${ARGN})
    if(_arg MATCHES "[^-./:a-zA-Z0-9_]")
      string(APPEND _args " [==[${_arg}]==]")  # form a bracket argument
    else()
      string(APPEND _args " ${_arg}")
    endif()
  endforeach()
  string(APPEND script "${NAME}(${_args})\n")
  string(LENGTH "${script}" _script_len)
  if(${_script_len} GREATER "50000")
    flush_script()
  endif()
  # Unsets macro local variables to prevent leakage outside of this macro.
  unset(_args)
  unset(_script_len)
endmacro()


# Adds another test to the script.
macro(add_another_test hierarchy_list enabled separator)
  # Create the name and path of the test-case...
  if (CMAKE_VERSION VERSION_LESS "3.12")
    set(test_name)
    set(test_path)
    foreach(hierarchy_entry IN LISTS ${hierarchy_list})
      if ("${test_name}" STREQUAL "")
        set(test_name "${hierarchy_entry}")
      else()
        set(test_name "${test_name}${separator}${hierarchy_entry}")
      endif()
      if ("${test_path}" STREQUAL "")
        set(test_path "${hierarchy_entry}")
      else()
        set(test_path "${test_path}/${hierarchy_entry}")
      endif()
    endforeach()
  else()
    list(JOIN ${hierarchy_list} ${separator} test_name)
    list(JOIN ${hierarchy_list} "/" test_path)
  endif()
  # ...and add to script.
  add_command(add_test
      "${prefix}${test_name}${suffix}"
      ${__TEST_EXECUTOR}
      "${__TEST_EXECUTABLE}"
      "--run_test=${test_path}"
      ${extra_args}
  )
  if(NOT ${enabled})
    add_command(set_tests_properties
      "${prefix}${test_name}${suffix}"
      PROPERTIES DISABLED TRUE
    )
  endif()
  add_command(set_tests_properties
    "${prefix}${test_name}${suffix}"
    PROPERTIES
    WORKING_DIRECTORY "${__TEST_WORKING_DIR}"
    ${properties}
  )
  list(APPEND tests_buffer "${prefix}${test_name}${suffix}")
  list(LENGTH tests_buffer tests_buffer_length)
  if(${tests_buffer_length} GREATER "250")
    flush_tests_buffer()
  endif()
endmacro()


# Internal implementation for boosttest_discover_tests.
function(boosttest_discover_tests_impl)

  cmake_parse_arguments(
    "_"
    ""
    "TEST_TARGET;TEST_EXECUTABLE;TEST_WORKING_DIR;TEST_PREFIX;TEST_SUFFIX;TEST_NAME_SEPARATOR;TEST_LIST;TEST_SKIP_DISABLED;CTEST_FILE;TEST_DISCOVERY_TIMEOUT"
    "TEST_EXTRA_ARGS;TEST_PROPERTIES;TEST_EXECUTOR"
    ${ARGN}
  )

  set(prefix "${__TEST_PREFIX}")
  set(suffix "${__TEST_SUFFIX}")
  set(extra_args ${__TEST_EXTRA_ARGS})
  set(properties ${__TEST_PROPERTIES})
  set(script)
  set(tests)
  set(tests_buffer)

  # Make sure the working directory exists.
  file(MAKE_DIRECTORY "${__TEST_WORKING_DIR}")
  # Run test executable to get list of available tests.
  if(NOT EXISTS "${__TEST_EXECUTABLE}")
    message(FATAL_ERROR
      "Specified test executable does not exist.\n"
      "  Path: '${__TEST_EXECUTABLE}'"
    )
  endif()
  execute_process(
    COMMAND ${__TEST_EXECUTOR} "${__TEST_EXECUTABLE}" --list_content=HRF
    WORKING_DIRECTORY "${__TEST_WORKING_DIR}"
    TIMEOUT ${__TEST_DISCOVERY_TIMEOUT}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output  # Boost.Test writes the requested content to stderr!
    RESULT_VARIABLE result
  )
  if(NOT ${result} EQUAL 0)
    string(REPLACE "\n" "\n    " output "${output}")
    message(FATAL_ERROR
      "Error running test executable.\n"
      "  Path: '${__TEST_EXECUTABLE}'\n"
      "  Result: ${result}\n"
      "  Output:\n"
      "    ${output}\n"
    )
  endif()

  # Preserve semicolon in test-parameters
  string(REPLACE [[;]] [[\;]] output "${output}")
  string(REPLACE "\n" ";" output "${output}")

  # The hierarchy and its depth-level of the test of the former line.
  set(hierarchy "${TEST_TARGET}_MISSING_TESTS")
  set(former_level NaN)
  set(test_is_enabled 0)

  # Parse output
  foreach(line ${output})
    # Determine the depth-level of the next test-hierarchy.
    # Note: Each new depth-level (except for the top one) is indented
    #       by 4 spaces. So we need to count the spaces.
    string(REGEX MATCH "^[ ]+" next_level "${line}")
    string(LENGTH "${next_level}" next_level)
    math(EXPR next_level "${next_level} / 4")

    # Add the test for the test-case from the former loop-run?
    if ((next_level LESS former_level) OR (next_level EQUAL former_level))
      # Add test-case to the script.
      add_another_test(hierarchy ${test_is_enabled} "${__TEST_NAME_SEPARATOR}")

      # Prepare the hierarchy list for the next test-case.
      math(EXPR diff "${former_level} - ${next_level}")
      foreach(i RANGE ${diff})
        if (CMAKE_VERSION VERSION_LESS "3.15")
          list(LENGTH hierarchy length)
          math(EXPR index "${length} - 1")
          list(REMOVE_AT hierarchy ${index})
        else()
          list(POP_BACK hierarchy)
        endif()
      endforeach()
    endif()
    if (former_level STREQUAL NaN)
      set(hierarchy "")  # Clear hierarchy, as we have at least one test.
    endif()
    set(former_level ${next_level})  # Store depth-level for next loop-run.

    # Extract the name of the next test suite/case and determine if enabled.
    # Note: A trailing '*' indicates that the test is enabled (by default).
    string(REGEX REPLACE ":( .*)?$" "" name "${line}")
    string(STRIP "${name}" name)
    if(name MATCHES "\\*$")
      set(test_is_enabled 1)
      string(REGEX REPLACE "\\*$" "" name "${name}")
    elseif(__TEST_SKIP_DISABLED)
      set(test_is_enabled 0)
    endif()

    # Sanitize name for further processing downstream:
    #  - escape \
    string(REPLACE [[\]] [[\\]] name "${name}")
    #  - escape ;
    string(REPLACE [[;]] [[\;]] name "${name}")
    #  - escape $
    string(REPLACE [[$]] [[\$]] name "${name}")

    # Add the name to the hierarchy list.
    list(APPEND hierarchy "${name}")
  endforeach()

  # Add last test-case to the script (if any).
  add_another_test(hierarchy ${test_is_enabled} "${__TEST_NAME_SEPARATOR}")


  # Create a list of all discovered tests, which users may use to e.g. set
  # properties on the tests.
  flush_tests_buffer()
  if (NOT former_level STREQUAL "NaN")
    add_command(set ${__TEST_LIST} ${tests})
  endif()

  # Write CTest script
  file(WRITE "${__CTEST_FILE}" "${script}")

  # Write CTest script
  flush_script()

endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
  # Note: Make sure to remove the outer layer of quotes that were added
  #       to preserve whitespace when handed over via cmdline.
  remove_outer_quotes(TEST_TARGET)
  remove_outer_quotes(TEST_EXECUTABLE)
  remove_outer_quotes(TEST_EXECUTOR)
  remove_outer_quotes(TEST_WORKING_DIR)
  remove_outer_quotes(TEST_EXTRA_ARGS)
  remove_outer_quotes(TEST_PROPERTIES)
  remove_outer_quotes(TEST_PREFIX)
  remove_outer_quotes(TEST_SUFFIX)
  remove_outer_quotes(TEST_NAME_SEPARATOR)
  remove_outer_quotes(TEST_LIST)
  remove_outer_quotes(TEST_SKIP_DISABLED)
  remove_outer_quotes(CTEST_FILE)
  remove_outer_quotes(TEST_DISCOVERY_TIMEOUT)

  boosttest_discover_tests_impl(
    TEST_TARGET ${TEST_TARGET}
    TEST_EXECUTABLE ${TEST_EXECUTABLE}
    TEST_EXECUTOR ${TEST_EXECUTOR}
    TEST_WORKING_DIR ${TEST_WORKING_DIR}
    TEST_PREFIX ${TEST_PREFIX}
    TEST_SUFFIX ${TEST_SUFFIX}
    TEST_NAME_SEPARATOR ${TEST_NAME_SEPARATOR}
    TEST_LIST ${TEST_LIST}
    TEST_SKIP_DISABLED ${TEST_SKIP_DISABLED}
    CTEST_FILE ${CTEST_FILE}
    TEST_DISCOVERY_TIMEOUT ${TEST_DISCOVERY_TIMEOUT}
    TEST_EXTRA_ARGS ${TEST_EXTRA_ARGS}
    TEST_PROPERTIES ${TEST_PROPERTIES}
  )
endif()
