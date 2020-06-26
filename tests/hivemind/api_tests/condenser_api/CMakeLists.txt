# Adding python tests to cmake
MESSAGE( STATUS "### Adding condenser_api python tests")

## get_account_reputations
ADD_API_TEST(condenser_api get_account_reputations steemit 10)

## get_account_votes
ADD_API_TEST(condenser_api get_account_votes alice)

## get_active_votes
ADD_API_TEST(condenser_api get_active_votes drakos open-letter-to-justin-sun-and-the-steem-community)

## get_blog
ADD_API_TEST(condenser_api get_blog drakos 0 1)

## get_blog_authors
ADD_API_TEST(condenser_api get_blog_authors drakos)

## get_blog_entries
ADD_API_TEST(condenser_api get_blog_entries tarazkp 0 10)

## get_comment_discussions_by_payout!
ADD_API_TEST(condenser_api get_comment_discussions_by_payout [{"tag":"photography","limit":10,"truncate_body":0}])

## get_content
ADD_API_TEST(condenser_api get_content tarazkp the-legacy-of-yolo)

## get_content_replies
ADD_API_TEST(condenser_api get_content_replies tarazkp the-legacy-of-yolo)

## get_discussions_by_active!
ADD_API_TEST(condenser_api get_discussions_by_active [{"tag":"photography","limit":10,"truncate_body":0}])

## get_discussions_by_author_before_date
ADD_API_TEST(condenser_api get_discussions_by_author_before_date flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 2020-03-01T00:00:00 1)

## get_discussions_by_blog!
ADD_API_TEST(condenser_api get_discussions_by_blog [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_discussions_by_cashout
ADD_API_TEST(condenser_api get_discussions_by_cashout [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_discussions_by_children
ADD_API_TEST(condenser_api get_discussions_by_children [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_discussions_by_comments
ADD_API_TEST(condenser_api get_discussions_by_comments tarazkp the-legacy-of-yolo 5)

## get_discussions_by_created
ADD_API_TEST(condenser_api get_discussions_by_created [{"tag":"blocktrades","limit":1}])

## get_discussions_by_feed
ADD_API_TEST(condenser_api get_discussions_by_feed steemtools steempeak introducing-peaklock-and-keys-management 3)

## get_discussions_by_hot
ADD_API_TEST(condenser_api get_discussions_by_hot [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_discussions_by_promoted
ADD_API_TEST(condenser_api get_discussions_by_promoted [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_discussions_by_trending
ADD_API_TEST(condenser_api get_discussions_by_trending [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_discussions_by_votes
ADD_API_TEST(condenser_api get_discussions_by_votes [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_feed
ADD_API_TEST(condenser_api get_feed steemit 0 1)

## get_feed_entries
ADD_API_TEST(condenser_api get_feed_entries steemit 0 1)

## get_follow_count
ADD_API_TEST(condenser_api get_follow_count steemmeupscotty)

## get_followers
ADD_API_TEST(condenser_api get_followers steemit null blog 10)

## get_following
ADD_API_TEST(condenser_api get_following steemit null blog 10)

## get_post_discussions_by_payout
ADD_API_TEST(condenser_api get_post_discussions_by_payout [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_reblogged_by
ADD_API_TEST(condenser_api get_reblogged_by tarazkp the-legacy-of-yolo)

## get_replies_by_last_update
ADD_API_TEST(condenser_api get_replies_by_last_update tarazkp the-legacy-of-yolo 10)

## get_tags_used_by_author
ADD_API_TEST(condenser_api get_tags_used_by_author steemit)

## get_trending_tags
ADD_API_TEST(condenser_api get_trending_tags blocktrades 1)







