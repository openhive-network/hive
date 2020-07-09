SET ( dhf_tests_args initminer initminer ${initminer_key} --run-hived ${hived_path} --working-dir ${CMAKE_BINARY_DIR}/tests/hive-node-data)

ADD_PYTHON_FUNCTIONAL_TEST( beem_dhf_tests ${dhf_tests_args} --no-erase-proposal )
ADD_PYTHON_FUNCTIONAL_TEST( list_proposals_tests ${dhf_tests_args} )
