SET ( rc_tests_args initminer ${initminer_key} --run-hived ${hived_path} --working-dir ${CMAKE_BINARY_DIR}/tests/hive-node-data)

IF ( BUILD_HIVE_TESTNET )
    ADD_PYTHON_FUNCTIONAL_TEST( beem_rc_test.py ${rc_tests_args} )
ELSE ( BUILD_HIVE_TESTNET )
    MESSAGE ( STATUS "RC functional tests skipped: BUILD_HIVE_TESTNET is required" )
ENDIF ( BUILD_HIVE_TESTNET )
