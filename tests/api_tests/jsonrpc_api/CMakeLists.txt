# Adding python tests to cmake
MESSAGE( STATUS "### Adding jsonrpc_api python tests")
## get_methods
ADD_API_TEST(jsonrpc_api get_methods)

## get_signature
ADD_API_TEST(jsonrpc_api get_signature market_history_api.get_market_history)
