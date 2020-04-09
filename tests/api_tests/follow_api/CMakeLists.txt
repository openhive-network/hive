# Adding python tests to cmake
MESSAGE( STATUS "### Adding follow_api python tests")

## get_account_reputations
ADD_API_TEST(follow_api get_account_reputations 1 blocktrades 1)

## get_blog
ADD_API_TEST(follow_api get_blog 1 blocktardes 0 1)

## get_blog_entries
ADD_API_TEST(follow_api get_blog_entries 1 blocktrades 0 1)

## get_follow_count
ADD_API_TEST(follow_api get_follow_count 1 blocktrades)

## get_followers
ADD_API_TEST(follow_api get_followers 1 steemit \"\" blog 10)

## get_following
ADD_API_TEST(follow_api get_following 1 blocktrades \"\" blog 10)

## get_reblogged_by
ADD_API_TEST(follow_api get_reblogged_by 1 steemit firstpost)

