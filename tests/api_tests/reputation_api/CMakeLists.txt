# Adding python tests to cmake
MESSAGE( STATUS "### Adding reputation_api python tests")
## get_account_reputations
ADD_API_TEST(reputation_api get_account_reputations 1 blocktrades 10)