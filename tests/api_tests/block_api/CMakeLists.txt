# Adding python tests to cmake
MESSAGE( STATUS "### Adding block_api python tests")
##get_block
ADD_API_TEST(block_api get_block 1000)

##get_block_header
ADD_API_TEST(block_api get_block_header 1000)
