INCLUDE ( CTest )
ENABLE_TESTING()

SET ( python_funtional_tests_prefix functional )
SET ( hived_path ${CMAKE_BINARY_DIR}/programs/hived/hived )
SET ( cli_wallet_path ${CMAKE_BINARY_DIR}/programs/cli_wallet )
SET ( initminer_key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n )

MACRO (__SETUP_TEST_VARS test_name )

  FILE( RELATIVE_PATH tests_subset "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}" )
  SET ( test_name ${python_funtional_tests_prefix}/${tests_subset}/${test_name} )
  SET ( working_dir ${CMAKE_CURRENT_SOURCE_DIR} )
  MESSAGE ( STATUS "Adding ${test_name} to test list" )

ENDMACRO( __SETUP_TEST_VARS )

# It is required to start ctest in ${CMAKE_BINARY_DIR}/tests
# The test are grouped as follow functional/<subdirectory name>/<test_basefile_name>
#   example: test script functional/python_tests/dhf_tests/beem_dhf_tests.py will started with ctest as follows:
#       ctest functional/dhf_tests/beem_dhf_tests
# To start group of tests use ctest -R
#   example: to start all tests in dhf_tests group:
#       ctest -R functional/dhf_tests/.*
#   example: to start all python_functional tests:
#       ctest -R functional/.*

MACRO ( ADD_PYTHON_FUNCTIONAL_TEST test_script_base_name )

    __SETUP_TEST_VARS( ${test_script_base_name} )
    SET ( test_script ${test_script_base_name}.py )

    SET ( extra_macro_args ${ARGN} )
    LIST ( LENGTH extra_macro_args num_extra_args )
    SET ( test_parameters ${test_script}  )

    IF ( ${num_extra_args} GREATER 0)
        SET ( test_parameters ${test_script} ${extra_macro_args} )
    ENDIF ()

    ADD_TEST( NAME ${test_name} COMMAND python3 ${test_parameters} WORKING_DIRECTORY ${working_dir} )
    SET_PROPERTY( TEST "${test_name}" PROPERTY LABELS ${python_funtional_tests_prefix} ${tests_subset} ${test_script_base_name} )
    SET_PROPERTY( TEST "${test_name}" PROPERTY ENVIRONMENT TEST_LOG_DIR=${CMAKE_BINARY_DIR}/tests)
    SET_PROPERTY( TEST "${test_name}" APPEND PROPERTY ENVIRONMENT PYTHONPATH=${CMAKE_SOURCE_DIR}/tests/functional)

ENDMACRO( ADD_PYTHON_FUNCTIONAL_TEST )

MACRO( ADD_PYTEST_TEST_GROUP )

  __SETUP_TEST_VARS( ${test_name} pytest )
  ADD_TEST( NAME ${test_name} COMMAND pytest . WORKING_DIRECTORY ${working_dir} )

ENDMACRO( ADD_PYTEST_TEST_GROUP )

# Add here directories with subsets of tests
ADD_SUBDIRECTORY ( dhf_tests )
ADD_SUBDIRECTORY ( hived )

