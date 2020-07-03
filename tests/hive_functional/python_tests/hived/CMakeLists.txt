SET ( hived_tests_args --run-hived ${hived_path})

ADD_PYTHON_FUNCTIONAL_TEST( hived_options_tests ${hived_tests_args} )
