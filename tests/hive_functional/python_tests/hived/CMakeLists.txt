SET ( hived_tests_args --run-hived ${hived_path})

IF ( BUILD_HIVE_TESTNET )
    ADD_PYTHON_FUNCTIONAL_TEST( hived_options_tests ${hived_tests_args} )
ELSE ( BUILD_HIVE_TESTNET )
    MESSAGE ( STATUS "hived functional tests skipped: BUILD_HIVE_TESTNET is required" )
ENDIF ( BUILD_HIVE_TESTNET )
