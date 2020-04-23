# Adding python tests to cmake
MESSAGE( STATUS "### Adding rc_api python tests")

## find_rc_accounts
ADD_API_TEST(rc_api find_rc_accounts 1 blocktrades,steemit)

## get_resource_params
ADD_API_TEST(rc_api get_resource_params 1)

## get_resource_pool
ADD_API_TEST(rc_api get_resource_pool 1)
