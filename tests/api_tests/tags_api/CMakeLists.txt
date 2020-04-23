# Adding python tests to cmake
MESSAGE( STATUS "### Adding tags_api python tests")

## get_active_votes
## Method not found (no differences).
ADD_API_TEST(tags_api get_active_votes flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3)

## get_comment_discussions_by_payout 
ADD_API_TEST(tags_api get_comment_discussions_by_payout blocktrades 1)

## get_content_replies
ADD_API_TEST(tags_api get_content_replies flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3)

## get_discussion
ADD_API_TEST(tags_api get_discussion steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography)

## get_discussions_by_active
## Method not found in API.
ADD_API_TEST(tags_api get_discussions_by_active blocktrades 1)

## get_discussions_by_author_before_date
ADD_API_TEST(tags_api get_discussions_by_author_before_date flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 2020-03-01T00:00:00 1)

## get_discussions_by_blog
### Returns a list of discussions by blog.
ADD_API_TEST(tags_api get_discussions_by_blog blocktrades 1)

## get_discussions_by_cashout
## Method not found.
ADD_API_TEST(tags_api get_discussions_by_cashout blocktrades 1)

## get_discussions_by_children
## Method not found.
ADD_API_TEST(tags_api get_discussions_by_children blocktrades 1)

## get_discussions_by_comments
ADD_API_TEST(tags_api get_discussions_by_comments tarazkp the-legacy-of-yolo 1)

## get_discussions_by_created
ADD_API_TEST(tags_api get_discussions_by_created blocktrades 1)

##get_discussions_by_feed
## Method not found.
ADD_API_TEST(tags_api get_discussions_by_feed blocktrades 1)

##get_discussions_by_hot. 
ADD_API_TEST(tags_api get_discussions_by_hot blocktrades 1)

## get_discussions_by_promoted
ADD_API_TEST(tags_api get_discussions_by_promoted blocktrades 1)

## get_discussions_by_trending
ADD_API_TEST(tags_api get_discussions_by_trending blocktrades 1)

## get_discussions_by_votes
## Method not found.
ADD_API_TEST(tags_api get_discussions_by_votes blocktrades 1)

## get_post_discussions_by_payout
ADD_API_TEST(tags_api get_post_discussions_by_payout blocktrades 1)

## get_replies_by_last_update
## Method not found
ADD_API_TEST(tags_api get_replies_by_last_update flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 1)
 
## get_tags_used_by_author
## Method not found
ADD_API_TEST(tags_api get_tags_used_by_author flaws)

## get_trending_tags
## Method not found
ADD_API_TEST(tags_api get_trending_tags blocktrades 1)




