# follow_api test notes

## get_account_reputations
`python3 get_account_reputations.py https://api.steem.house https://api.steemit.com ./ 1 blocktrades 1`

Expeced result: same result on both nodes

## get_blog
`python3 get_blog.py https://api.steem.house https://api.steemit.com ./ 1 blocktardes 0 1`

**Expeced result: Differences found at post_id and json_metadata**

## get_blog_authors

Calling method follow_api.get_blog_authors returns error - {"code":-32601,"message":"Method not found","data":"follow_api.get_blog_authors"}

## get_blog_entries
`python3 get_blog_entries.py https://api.steem.house https://api.steemit.com ./ 1 blocktrades 0 1`

Expeced result: same result on both nodes

## get_feed

Calling method follow_api.get_feed returns error - {"code":-32601,"message":"Method not found","data":"follow_api.get_feed"}

## get_feed_entries

Calling method follow_api.get_feed_entries returns error - {"code":-32601,"message":"Method not found","data":"follow_api.get_feed_entries"}

## get_follow_count
`python3 get_follow_count.py https://api.steem.house https://api.steemit.com ./ 1 blocktrades`

**Expeced result: Differences found at follower_count**

## get_followers
`python3 get_followers.py https://api.steem.house https://api.steemit.com ./ 1 steemit '' blog 10`

Expeced result: same result on both nodes

## get_following
`python3 get_following.py https://api.steem.house https://api.steemit.com ./ 1 blocktrades '' blog 10`

Expeced result: same result on both nodes

## get_reblogged_by
`python3 get_reblogged_by.py https://api.steem.house https://api.steemit.com ./ 1 steemit firstpost`

Expeced result: same result on both nodes

