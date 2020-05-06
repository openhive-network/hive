# Adding python tests to cmake
MESSAGE( STATUS "### Adding bridge python tests")
## account_notifications
ADD_API_TEST(bridge account_notifications steemmeupscotty 1)

## get_community
ADD_API_TEST(bridge get_community hive-123456 alice)

## get_ranked_posts
ADD_API_TEST(bridge get_ranked_posts trending hive alice)

## list_all_subscriptions
ADD_API_TEST(bridge list_all_subscriptions alice)

## list_community_roles
ADD_API_TEST(bridge list_community_roles blocktrades)
