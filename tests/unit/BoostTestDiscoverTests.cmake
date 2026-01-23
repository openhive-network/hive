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

#[=======================================================================[.rst:
BoostTestDiscoverTests
----------------------

This module defines a function to help use the Boost.Test infrastructure.

The :command:`boosttest_discover_tests` discovers tests by asking the compiled
test executable to enumerate its tests.  This does not require CMake to be
re-run when tests change.  However, it may not work in a cross-compiling
environment, and setting test properties is less convenient.

This command is intended to replace use of :command:`add_test` to register
tests, and will create a separate CTest test for each Boost.Test test case.
Note that this is in some cases less efficient, as common set-up and tear-down
logic cannot be shared by multiple test cases executing in the same instance.
However, it provides more fine-grained pass/fail information to CTest, which is
usually considered as more beneficial.  By default, the CTest test name is the
same as the Boost.Test name; see also ``TEST_PREFIX`` and ``TEST_SUFFIX``.

.. command:: boosttest_discover_tests

  Automatically add tests with CTest by querying the compiled test executable
  for available tests::

    boosttest_discover_tests(target
                             [EXTRA_ARGS arg1...]
                             [WORKING_DIRECTORY dir]
                             [TEST_PREFIX prefix]
                             [TEST_SUFFIX suffix]
                             [TEST_NAME_SEPARATOR separator]
                             [PROPERTIES name1 value1...]
                             [TEST_LIST var]
                             [SKIP_DISABLED_TESTS]
                             [DISCOVERY_TIMEOUT seconds]
                             [DISCOVERY_MODE <POST_BUILD|PRE_TEST>]
    )

  ``boosttest_discover_tests`` sets up a post-build / pre-test command on the
  test executable that generates the list of tests by parsing the output from
  running the test with the ``--list_content=HRF`` argument.  This ensures that
  the full list of tests, including instantiations of parameterized tests, is
  obtained.  Since test discovery occurs at build time (or pre-test time), it
  is not necessary to re-run CMake when the list of tests changes.
  However, it requires that :prop_tgt:`CROSSCOMPILING_EMULATOR` is properly set
  in order to function in a cross-compiling environment.

  Additionally, setting properties on tests is somewhat less convenient, since
  the tests are not available at CMake time.  Additional test properties may be
  assigned to the set of tests as a whole using the ``PROPERTIES`` option.  If
  more fine-grained test control is needed, custom content may be provided
  through an external CTest script using the :prop_dir:`TEST_INCLUDE_FILES`
  directory property.  The set of discovered tests is made accessible to such a
  script via the ``<target>_TESTS`` variable.

  The options are:

  ``target``
    Specifies the Boost.Test executable, which must be a known CMake executable
    target.  CMake will substitute the location of the built executable when
    running the test.

  ``EXTRA_ARGS arg1...``
    Any extra arguments to pass on the command line to each test case.

  ``WORKING_DIRECTORY dir``
    Specifies the directory in which to run the discovered test cases.  If this
    option is not provided, the current binary directory is used.
    Note, that if this directory does not exist it will be created.

  ``TEST_PREFIX prefix``
    Specifies a ``prefix`` to be prepended to the name of each discovered test
    case.  This can be useful when the same test executable is being used in
    multiple calls to ``boosttest_discover_tests()`` but with different
    ``EXTRA_ARGS``.

  ``TEST_SUFFIX suffix``
    Similar to ``TEST_PREFIX`` except the ``suffix`` is appended to the name of
    every discovered test case.  Both ``TEST_PREFIX`` and ``TEST_SUFFIX`` may
    be specified.

  ``TEST_NAME_SEPARATOR``
    With Boost.Test a test case can be continued in one or more test suites.
    The names of the test suites and the test case will be joined into a full
    name.  By default they will be joined with a ``.`` character between them.
    However, this option allows to use another character (or string) as
    separator between the individual name parts.
    Note however, that he ``TEST_PREFIX`` and ``TEST_SUFFIX`` will be
    prepended/appended to this full name without an extra separator.

  ``PROPERTIES name1 value1...``
    Specifies additional properties to be set on all tests discovered by this
    invocation of ``boosttest_discover_tests``.

  ``TEST_LIST var``
    Make the list of tests available in the variable ``var``, rather than the
    default ``<target>_TESTS``.  This can be useful when the same test
    executable is being used in multiple calls to
    ``boosttest_discover_tests()``.  Note that this variable is only available
    in CTest.

  ``SKIP_DISABLED_TESTS``
    Specifies if disabled tests should be skipped. Boost.Test allows to disable
    individual tests which in general would not be run by default but because
    ``boosttest_discover_tests`` finds all tests and runs each one individually
    we have to explicitly skip these with this option in case they shall not be
    run.

  ``DISCOVERY_TIMEOUT num``
    Specifies how long (in seconds) CMake will wait for the test to enumerate
    available tests.  If the test takes longer than this, discovery (and your
    build) will fail.  Most test executables will enumerate their tests very
    quickly, but under some exceptional circumstances, a test may require a
    longer timeout.  The default is 5.  See also the ``TIMEOUT`` option of
    :command:`execute_process`.

  ``DISCOVERY_MODE``
    Provides greater control over when ``boosttest_discover_tests``performs
    test discovery. By default, ``PRE_TEST`` delays test discovery until just
    prior to test execution. This way test discovery occurs in the target
    environment where the test has a better chance at finding appropriate
    runtime dependencies.
    By contrast, ``POST_BUILD`` sets up a post-build command to perform test
    discovery at build time. In certain scenarios, like cross-compiling, this
    ``POST_BUILD`` behavior is not desirable.

  ``DISCOVERY_MODE`` defaults to the value of the
    ``BOOSTTEST_DISCOVER_TESTS_DISCOVERY_MODE`` variable if it is not passed
    when calling ``boosttest_discover_tests``. This provides a mechanism for
    globally selecting a preferred test discovery behavior without having to
    modify each call site.

#]=======================================================================]

if(CMAKE_VERSION VERSION_LESS "3.5")
  message(FATAL_ERROR "CMake version is too old for `boosttest_discover_tests`!")
endif()

if(CMAKE_VERSION VERSION_LESS "3.17")
  set(__BOOSTTEST_DISCOVER_TESTS_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

#------------------------------------------------------------------------------
function(boosttest_discover_tests TARGET)

  cmake_parse_arguments(
    "_"
    "SKIP_DISABLED_TESTS"
    "WORKING_DIRECTORY;TEST_PREFIX;TEST_SUFFIX;TEST_NAME_SEPARATOR;TEST_LIST;DISCOVERY_TIMEOUT;DISCOVERY_MODE"
    "EXTRA_ARGS;PROPERTIES"
    ${ARGN}
  )

  if(NOT __WORKING_DIRECTORY)
    set(__WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endif()
  if(NOT __TEST_NAME_SEPARATOR)
    set(__TEST_NAME_SEPARATOR ".")
  endif()
  if(NOT __TEST_LIST)
    set(__TEST_LIST ${TARGET}_TESTS)
  endif()
  if(NOT __DISCOVERY_TIMEOUT)
    set(__DISCOVERY_TIMEOUT 5)
  endif()
  if(NOT __DISCOVERY_MODE)
    if(NOT BOOSTTEST_DISCOVER_TESTS_DISCOVERY_MODE)
      set(BOOSTTEST_DISCOVER_TESTS_DISCOVERY_MODE "PRE_TEST")
    endif()
    set(__DISCOVERY_MODE ${BOOSTTEST_DISCOVER_TESTS_DISCOVERY_MODE})
  endif()

  # Store the absolute path to the helper script.
  set(__BOOSTTEST_DISCOVER_TESTS_SCRIPT
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BoostTestAddTests.cmake"
  )
  if(CMAKE_VERSION VERSION_LESS "3.17")
    set(__BOOSTTEST_DISCOVER_TESTS_SCRIPT
      "${__BOOSTTEST_DISCOVER_TESTS_DIR}/BoostTestAddTests.cmake"
    )
  endif()

  # Determine how often tests were discovered on the target and store in property
  get_property(
    has_counter
    TARGET ${TARGET}
    PROPERTY CTEST_DISCOVERED_TEST_COUNTER
    SET
  )
  if(has_counter)
    get_property(
      counter
      TARGET ${TARGET}
      PROPERTY CTEST_DISCOVERED_TEST_COUNTER
    )
    math(EXPR counter "${counter} + 1")
  else()
    set(counter 1)
  endif()
  set_property(
    TARGET ${TARGET}
    PROPERTY CTEST_DISCOVERED_TEST_COUNTER
    ${counter}
  )

  get_property(crosscompiling_emulator
    TARGET ${TARGET}
    PROPERTY CROSSCOMPILING_EMULATOR
  )

  get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL
      PROPERTY GENERATOR_IS_MULTI_CONFIG
  )

  # Prepare file names for ctest configuration files.
  set(ctest_file_base "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}[${counter}]")
  set(ctest_include_file "${ctest_file_base}_include.cmake")
  if(GENERATOR_IS_MULTI_CONFIG)
    set(ctest_tests_file "${ctest_file_base}_tests-$<CONFIG>.cmake")
  else()
    set(ctest_tests_file "${ctest_file_base}_tests.cmake")
  endif()

  # Define rule to generate test list for aforementioned test executable

  if(__DISCOVERY_MODE STREQUAL "POST_BUILD")

    if(CMAKE_VERSION VERSION_LESS "3.20" AND GENERATOR_IS_MULTI_CONFIG)
      message(WARNING "CMake is too old to support discovery mode POST_BUILD with multi-config generators. Consider using discovery mode PRE_TEST instead!")
      set(ctest_tests_file "${ctest_file_base}_tests.cmake")
    endif()

    # Note: Make sure to properly quote all definitions that are given
    #       via the cmdline, or we might swallow some white-space.
    add_custom_command(
      TARGET ${TARGET} POST_BUILD
      BYPRODUCTS "${ctest_tests_file}"
      COMMAND "${CMAKE_COMMAND}"
              -D "TEST_TARGET=\"${TARGET}\""
              -D "TEST_EXECUTABLE=\"$<TARGET_FILE:${TARGET}>\""
              -D "TEST_EXECUTOR=\"${crosscompiling_emulator}\""
              -D "TEST_WORKING_DIR=\"${__WORKING_DIRECTORY}\""
              -D "TEST_EXTRA_ARGS=\"${__EXTRA_ARGS}\""
              -D "TEST_PROPERTIES=\"${__PROPERTIES}\""
              -D "TEST_PREFIX=\"${__TEST_PREFIX}\""
              -D "TEST_SUFFIX=\"${__TEST_SUFFIX}\""
              -D "TEST_NAME_SEPARATOR=\"${__TEST_NAME_SEPARATOR}\""
              -D "TEST_LIST=\"${__TEST_LIST}\""
              -D "TEST_SKIP_DISABLED=\"${__SKIP_DISABLED_TESTS}\""
              -D "CTEST_FILE=\"${ctest_tests_file}\""
              -D "TEST_DISCOVERY_TIMEOUT=\"${__DISCOVERY_TIMEOUT}\""
              -P "${__BOOSTTEST_DISCOVER_TESTS_SCRIPT}"
      VERBATIM
    )
    string(CONCAT ctest_include_content
      "if(EXISTS \"${ctest_tests_file}\")"                    "\n"
      "  include(\"${ctest_tests_file}\")"                    "\n"
      "else()"                                                "\n"
      "  add_test(${TARGET}_NOT_BUILT ${TARGET}_NOT_BUILT)"   "\n"
      "endif()"                                               "\n"
    )

  elseif(__DISCOVERY_MODE STREQUAL "PRE_TEST")

    string(CONCAT ctest_include_content
      "if(EXISTS \"$<TARGET_FILE:${TARGET}>\")"                                    "\n"
      "  if(\"$<TARGET_FILE:${TARGET}>\" IS_NEWER_THAN \"${ctest_tests_file}\")"   "\n"
      "    include(\"${__BOOSTTEST_DISCOVER_TESTS_SCRIPT}\")"                      "\n"
      "    boosttest_discover_tests_impl("                                         "\n"
      "      TEST_TARGET"            " [==[" "${TARGET}"                  "]==]"   "\n"
      "      TEST_EXECUTABLE"        " [==[" "$<TARGET_FILE:${TARGET}>"   "]==]"   "\n"
      "      TEST_EXECUTOR"          " [==[" "${crosscompiling_emulator}" "]==]"   "\n"
      "      TEST_WORKING_DIR"       " [==[" "${__WORKING_DIRECTORY}"     "]==]"   "\n"
      "      TEST_EXTRA_ARGS"        " [==[" "${__EXTRA_ARGS}"            "]==]"   "\n"
      "      TEST_PROPERTIES"        " [==[" "${__PROPERTIES}"            "]==]"   "\n"
      "      TEST_PREFIX"            " [==[" "${__TEST_PREFIX}"           "]==]"   "\n"
      "      TEST_SUFFIX"            " [==[" "${__TEST_SUFFIX}"           "]==]"   "\n"
      "      TEST_NAME_SEPARATOR"    " [==[" "${__TEST_NAME_SEPARATOR}"   "]==]"   "\n"
      "      TEST_LIST"              " [==[" "${__TEST_LIST}"             "]==]"   "\n"
      "      TEST_SKIP_DISABLED"     " [==[" "${__SKIP_DISABLED_TESTS}"   "]==]"   "\n"
      "      CTEST_FILE"             " [==[" "${ctest_tests_file}"        "]==]"   "\n"
      "      TEST_DISCOVERY_TIMEOUT" " [==[" "${__DISCOVERY_TIMEOUT}"     "]==]"   "\n"
      "    )"                                                                      "\n"
      "  endif()"                                                                  "\n"
      "  include(\"${ctest_tests_file}\")"                                         "\n"
      "else()"                                                                     "\n"
      "  add_test(${TARGET}_NOT_BUILT ${TARGET}_NOT_BUILT)"                        "\n"
      "endif()"                                                                    "\n"
    )

  else()
    message(SEND_ERROR "Unknown DISCOVERY_MODE: ${__DISCOVERY_MODE}")
  endif()

  if(GENERATOR_IS_MULTI_CONFIG)
    foreach(_config ${CMAKE_CONFIGURATION_TYPES})
      file(GENERATE OUTPUT "${ctest_file_base}_include-${_config}.cmake" CONTENT "${ctest_include_content}" CONDITION $<CONFIG:${_config}>)
    endforeach()
    file(WRITE "${ctest_include_file}" "include(\"${ctest_file_base}_include-\${CTEST_CONFIGURATION_TYPE}.cmake\")")
  else()
    file(GENERATE OUTPUT "${ctest_file_base}_include.cmake" CONTENT "${ctest_include_content}")
  endif()

  # Add discovered tests to directory TEST_INCLUDE_FILES
  set_property(DIRECTORY
    APPEND PROPERTY TEST_INCLUDE_FILES "${ctest_include_file}"
  )

endfunction()

###############################################################################
