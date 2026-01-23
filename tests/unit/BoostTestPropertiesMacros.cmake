# Common macros for test property files.

if(NOT COMMAND set_test_properties_if_exists)
  macro(set_test_properties_if_exists TEST_NAME)
    if(TEST ${TEST_NAME})
      set_tests_properties(${TEST_NAME} ${ARGN})
    endif()
  endmacro()
endif()
