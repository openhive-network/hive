# Adding python tests to cmake
MESSAGE( STATUS "### Adding account_history_api python tests")
## get_account_history
ADD_API_TEST(account_history_api get_account_history steemmeupscotty 10 10)

## get_ops_in_block
ADD_API_TEST(account_history_api get_ops_in_block 100004)

## get_transaction
## DEPRECATED
# ADD_API_TEST(account_history_api get_transaction 0000000000000000000000000000000000000000)

## enum_virtual_ops
ADD_API_TEST(account_history_api enum_virtual_ops 9000 9100)
