SET ( hived_tests_args --run-hived ${hived_path})

ADD_PYTHON_FUNCTIONAL_TEST( hived_options_tests ${hived_tests_args} )
ADD_PYTHON_FUNCTIONAL_TEST( hived_hybrid_operations_test initminer ${initminer_key} ${hived_tests_args} )

ADD_PYTHON_FUNCTIONAL_TEST( hived_options_tests ${hived_tests_args} )

