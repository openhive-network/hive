SET ( dhf_tests_args initminer initminer ${initminer_key} --run-hived ${hived_path} --working-dir ${CMAKE_BINARY_DIR}/tests/hive-node-data)

IF ( BUILD_HIVE_TESTNET )
    ADD_PYTHON_FUNCTIONAL_TEST( beem_dhf_tests ${dhf_tests_args} --no-erase-proposal )
    ADD_PYTHON_FUNCTIONAL_TEST( list_proposals_tests ${dhf_tests_args} )
ELSE ( BUILD_HIVE_TESTNET )
    MESSAGE ( STATUS "DHF functional tests skipped: BUILD_HIVE_TESTNET is required" )
ENDIF ( BUILD_HIVE_TESTNET )