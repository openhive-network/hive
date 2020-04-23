# condenser_api methods
- broadcast_block 
- broadcast_transaction
- broadcast_transaction_synchronous
- find_proposals
- get_account_bandwidth
- get_account_count
- get_account_history
- get_account_references
- get_account_reputations
- get_account_votes
- get_accounts
- get_active_votes
- get_active_witnesses
- get_block
- get_block_header
- get_blog
- get_blog_authors
- get_blog_entries
- get_chain_properties
- get_comment_discussions_by_payout
- get_config
- get_content
- get_content_replies
- get_conversion_requests
- get_current_median_history_price
- get_discussions_by_active
- get_discussions_by_author_before_date
- get_discussions_by_blog
- get_discussions_by_cashout
- get_discussions_by_children
- get_discussions_by_comments
- get_discussions_by_created
- get_discussions_by_feed
- get_discussions_by_hot
- get_discussions_by_promoted
- get_discussions_by_trending
- get_discussions_by_votes
- get_dynamic_global_properties
- get_escrow
- get_expiring_vesting_delegations
- get_feed
- get_feed_entries
- get_feed_history
- get_follow_count
- get_followers
- get_following
- get_hardfork_version
- get_key_references
- get_market_history
- get_market_history_buckets
- get_next_scheduled_hardfork
- get_open_orders
- get_ops_in_block
- get_order_book
- get_owner_history
- get_post_discussions_by_payout
- get_potential_signatures
- get_reblogged_by
- get_recent_trades
- get_recovery_request
- get_replies_by_last_update
- get_required_signatures
- get_reward_fund
- get_savings_withdraw_from
- get_savings_withdraw_to
- get_state
- get_tags_used_by_author
- get_ticker
- get_trade_history
- get_transaction
- get_transaction_hex
- get_trending_tags
- get_version
- get_vesting_delegations
- get_volume
- get_withdraw_routes 
- get_witness_by_account
- get_witness_count
- get_witness_schedule
- get_witnesses
- get_witnesses_by_vote
- list_proposal_votes
- list_proposals
- lookup_account_names
- lookup_accounts
- lookup_witness_accounts
- verify_account_authority
- verify_authority 
 
 ```parser.add_argument("account_names", type = str, nargs='+', help = "Names of the accounts")```
 

#### broadcast_block 
Linux call:
`python3 broadcast_block.py https://api.steem.house https://api.steemit.com ./ '[{"previous":"0000000000000000000000000000000000000000","timestamp":"1970-01-01T00:00:00","witness":"","transaction_merkle_root":"0000000000000000000000000000000000000000","extensions":[],"witness_signature":"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","transactions":[]}]' 
Win call:
`python broadcast_block.py https://api.steemit.com https://api.steem.house ./ "[{\"previous\":\"0000000000000000000000000000000000000000\",\"timestamp\":\"1970-01-01T00:00:00\",\"witness\":\"\",\"transaction_merkle_root\":\"0000000000000000000000000000000000000000\",\"extensions\":[],\"witness_signature\":\"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\",\"transactions\":[]}]"
Results:
Response from test node at https://api.steemit.com:
```
 "stack": [
                {
                    "context": {
                        "file": "fork_database.cpp",
                        "hostname": "",
                        "level": "error",
                        "line": 57,
                        "method": "_push_block",
                        "timestamp": "2020-03-12T14:05:24"
                    },
                    "data": {
                        "head": 41588841,
                        "item->num": 1,
                        "max_size": 16
                    },
                    "format": "item->num > std::max<int64_t>( 0, int64_t(_head->num) - (_max_size) ): attempting to push a block that is too old"
                },

```
Response from ref node at https://api.steem.house:
`"_network_broadcast_api: network_broadcast_api_plugin not enabled.

#### broadcast_transaction
Linux call:
`python3 broadcast_transaction.py https://api.steem.house https://api.steemit.com ./ '[{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["vote",{"voter":"steemit","author":"alice","permlink":"a-post-by-alice","weight":10000}]],"extensions":[],"signatures":[]}]'

Windows call:
`python broadcast_transaction.py https://api.steemit.com https://api.steem.house ./ "[{\"ref_block_num\":1097,\"ref_block_prefix\":2181793527,\"expiration\":\"2016-03-24T18:00:21\",\"operations\":[[\"vote\",{\"voter\":\"steemit\",\"author\":\"alice\",\"permlink\":\"a-post-by-alice\",\"weight\":10000}]],\"extensions\":[],\"signatures\":[]}]"

Results:
`"missing required posting authority:Missing Posting Authority steemit"

#### broadcast_transaction_synchronous
Linux call:
`python3 broadcast_transaction_synchronous.py https://api.steem.house https://api.steemit.com ./ '[{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["vote",{"voter":"steemit","author":"alice","permlink":"a-post-by-alice","weight":10000}]],"extensions":[],"signatures":[]}]'

Windows call:
`python broadcast_transaction_synchronous.py https://api.steemit.com https://api.steem.house ./ "[{\"ref_block_num\":1097,\"ref_block_prefix\":2181793527,\"expiration\":\"2016-03-24T18:00:21\",\"operations\":[[\"vote\",{\"voter\":\"steemit\",\"author\":\"alice\",\"permlink\":\"a-post-by-alice\",\"weight\":10000}]],\"extensions\":[],\"signatures\":[]}]"

Results:
`"missing required posting authority:Missing Posting Authority steemit"

#### find_proposals
Linux call:
`python3 find_proposals.py https://api.steem.house https://api.steemit.com ./ 0

Windows call:
`python find_proposals.py https://api.steemit.com https://api.steem.house ./ 0

Test node https://api.steemit.com returned error code
Reference node https://api.steem.house returned error code
Compare finished

```
Response from test node at https://api.steemit.com: & https://api.steem.house:
{
    "error": {
        "code": -32000,
        "data": {
            "code": 7,
            "message": "Bad Cast",
            "name": "bad_cast_exception",
            "stack": [
                {
                    "context": {
                        "file": "variant.cpp",
                        "hostname": "",
                        "level": "error",
                        "line": 537,
                        "method": "get_array",
                        "timestamp": "2020-03-11T14:06:12"
                    },
                    "data": {
                        "type": "object_type"
                    },
                    "format": "Invalid cast from ${type} to Array"
                }
            ]
        },
        "message": "Bad Cast:Invalid cast from object_type to Array"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```

#### get_account_bandwidth
Its disabled since 0.20.6 https://developers.steem.io/apidefinitions/#condenser_api.get_account_bandwidth

#### get_account_count
Linux call:
`python3 get_account_count.py https://api.steem.house https://api.steemit.com ./

Windows call:
`python get_account_count.py https://api.steemit.com https://api.steem.house ./

Works fine

#### get_account_history
Linux call:
`python3 get_account_history.py https://api.steem.house https://api.steemit.com ./ steemit 1000 1000

Windows call:
`python get_account_history.py https://api.steemit.com https://api.steem.house ./ steemit 1000 1000

Works fine

#### get_account_references
Not implemented https://developers.steem.io/apidefinitions/#condenser_api.get_account_references

#### get_account_reputations
Linux call:
`python3 get_account_reputations.py https://api.steem.house https://api.steemit.com ./ steemit 1000

Windows call:
`python get_account_reputations.py https://api.steemit.com https://api.steem.house ./ steemit 1000

Results:
Looks good, still dont know if reputation like this is valid 
```
"account": "steemitdonate",
"reputation": -47284605688
```

#### get_account_votes
Linux call:
`python3 get_account_votes.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_account_votes.py https://api.steemit.com https://api.steem.house ./ 

Results:
`"ApiError: get_account_votes is no longer supported, for details see https://steemit.com/steemit/@steemitdev/additional-public-api-change"

#### get_accounts
Linux call:
`python3 get_accounts.py https://api.steem.house https://api.steemit.com ./ steemit alice steempeak

Windows call:
`python get_accounts.py https://api.steemit.com https://api.steem.house ./ steemit alice steempeak

Results:
Works fine.

#### get_active_votes
Linux call:
`python3 get_active_votes.py https://api.steem.house https://api.steemit.com ./ drakos open-letter-to-justin-sun-and-the-steem-community

Windows call:
`python get_active_votes.py https://api.steemit.com https://api.steem.house ./ drakos open-letter-to-justin-sun-and-the-steem-community

Results:
Works good

#### get_active_witnesses
Linux call:
`python3 get_active_witnesses.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_active_witnesses.py https://api.steemit.com https://api.steem.house ./ 

Results:
Works fine.

#### get_block
Linux call:
`python3 get_block.py https://api.steem.house https://api.steemit.com ./ 1234

Windows call:
`python get_block.py https://api.steemit.com https://api.steem.house ./ 1234

Results:
Works good.

#### get_block_header
Linux call:
`python3 get_block_header.py https://api.steem.house https://api.steemit.com ./ 123454

Windows call:
`python get_block_header.py https://api.steemit.com https://api.steem.house ./ 123454

Results:
Looks fine

#### get_blog
Linux call:
`python3 get_blog.py https://api.steem.house https://api.steemit.com ./ drakos 0 1

Windows call:
`python get_blog.py https://api.steemit.com https://api.steem.house ./ drakos 0 1

Note:
With different parameter combinations like(drakos 0 10) even with (drakos 0 2) i got:
`RecursionError: maximum recursion depth exceeded in comparison

Results:
```
Differences detected in jsons: {
    "result": {
        "0": {
            "comment": {
                "post_id": 85153588
            }
        }
    }
}
```

#### get_blog_authors
Linux call:
`python3 get_blog_authors.py https://api.steem.house https://api.steemit.com ./ drakos

Windows call:
`python get_blog_authors.py https://api.steemit.com https://api.steem.house ./ drakos

Note:
sometimes it reports a diff in timestamp it should be ignored as it's a 1 second difference

Results:
`"Assert Exception:_follow_api: follow_api_plugin not enabled."

#### get_blog_entries
Linux call:
`python3 get_blog_entries.py https://api.steem.house https://api.steemit.com ./ tarazkp 0 150

Windows call:
`python get_blog_entries.py https://api.steemit.com https://api.steem.house ./ tarazkp 0 150

Results:
Works fine.

#### get_chain_properties
Linux call:
`python3 get_chain_properties.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_chain_properties.py https://api.steemit.com https://api.steem.house ./ 

Results:
Works fine.

#### get_comment_discussions_by_payout!
Linux call:
`python3 get_comment_discussions_by_payout.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"photography","limit":10,"truncate_body":0}]'
Windows call:
`python get_comment_discussions_by_payout.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"photography\",\"limit\":10,\"truncate_body\":0}]"

Results:
Differences found in: `"post_id"

Notes:
I took parameters from curl examples in api docs but in this case it looks like only 3 parameters are supported:
`tag
`limit
`truncate_body

I tried to run with additional parameter `select_authors` calls looked like this:
Linux call:
`python3 get_comment_discussions_by_payout.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"photography","limit":10,"select_authors":["steemcleaners"],"truncate_body":0}]'
Windows call:
`python get_comment_discussions_by_payout.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"photography\",\"limit\":10,\"select_authors\":[\"steemcleaners\"],\"truncate_body\":0}]"
It resulted with:
`"get_comment_discussions_by_payout() got an unexpected keyword argument 'select_authors'"

#### get_config
Linux call:
`python3 get_config.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_config.py https://api.steemit.com https://api.steem.house ./ 

Results:
Looks fine

#### get_content
Linux call:
`python3 get_content.py https://api.steem.house https://api.steemit.com ./ tarazkp the-legacy-of-yolo

Windows call:
`python get_content.py https://api.steemit.com https://api.steem.house ./ tarazkp the-legacy-of-yolo

Results:
Looks good.


#### get_content_replies
Linux call:
`python3 get_content_replies.py https://api.steem.house https://api.steemit.com ./ tarazkp the-legacy-of-yolo
`python3 get_content_replies.py https://api.steem.house https://api.steemit.com ./ steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography

Windows call:
`python get_content_replies.py https://api.steemit.com https://api.steem.house ./ tarazkp the-legacy-of-yolo
`python get_content_replies.py https://api.steemit.com https://api.steem.house ./ steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography

Results:
Looks good.

#### get_conversion_requests
Linux call:
`python3 get_conversion_requests.py https://api.steem.house https://api.steemit.com ./ 1232345

Windows call:
`python get_conversion_requests.py https://api.steemit.com https://api.steem.house ./ 1232345

Results:
```
{
    "id": 1,
    "jsonrpc": "2.0",
    "result": []
}
```

#### get_current_median_history_price
Linux call:
`python3 get_current_median_history_price.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_current_median_history_price.py https://api.steemit.com https://api.steem.house ./ 

Results:
Works fine.

#### get_discussions_by_active!
Linux call:
`python3 get_discussions_by_active.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"photography","limit":10,"truncate_body":0}]'
Windows call:
`python get_discussions_by_active.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"photography\",\"limit\":10,\"truncate_body\":0}]"

Results:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."

Note:
It's most propably same case as `get_comment_discussions_by_payout` but i cant check it out because of:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."

Linux call with additional parameter:
`python3 get_discussions_by_active.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"photography","limit":10,"select_authors":["steemcleaners"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_active.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"photography\",\"limit\":10,\"select_authors\":[\"steemcleaners\"],\"truncate_body\":0}]"

#### get_discussions_by_author_before_date
Linux call:
`python3 get_discussions_by_author_before_date.py https://api.steem.house https://api.steemit.com ./ flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 2020-03-01T00:00:00 1

Windows call:
`python get_discussions_by_author_before_date.py https://api.steemit.com https://api.steem.house ./ flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 2020-03-01T00:00:00 1

Results:
Difference in: `"post_id"

#### get_discussions_by_blog!
Linux call:
`python3 get_discussions_by_blog.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_blog.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
Differences in:
`"post_id"

Note:
It's most propably same case as `get_comment_discussions_by_payout`

Linux call with additional parameter:
`python3 get_discussions_by_blog.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"select_authors":["steemcleaners"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_blog.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"select_authors\":[\"steemcleaners\"],\"truncate_body\":0}]"
Result:
`"get_discussions_by_blog() got an unexpected keyword argument 'select_authors'"

#### get_discussions_by_cashout
Linux call:
`python3 get_discussions_by_cashout.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_cashout.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."

Note:
It's most propably same case as `get_comment_discussions_by_payout` but i cannot check it out as:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."` occurs

Linux call with additional parameter:
`python3 get_discussions_by_cashout.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"select_authors":["steemcleaners"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_cashout.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"select_authors\":[\"steemcleaners\"],\"truncate_body\":0}]"

#### get_discussions_by_children
Linux call:
`python3 get_discussions_by_children.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_children.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."

Note:
It's most propably same case as `get_comment_discussions_by_payout` but i cannot check it out as:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."` occurs

Linux call with additional parameter:
`python3 get_discussions_by_children.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"select_authors":["steemcleaners"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_children.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"select_authors\":[\"steemcleaners\"],\"truncate_body\":0}]"

#### get_discussions_by_comments
Linux call:
`python3 get_discussions_by_comments.py https://api.steem.house https://api.steemit.com ./ tarazkp the-legacy-of-yolo 5

Windows call:
`python get_discussions_by_comments.py https://api.steemit.com https://api.steem.house ./ tarazkp the-legacy-of-yolo 5

Results:
Differences in: `"post_id"

#### get_discussions_by_created
Linux call:
`python3 get_discussions_by_created.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_created.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
Response from ref node at https://api.steem.house:
->"reputation": 0
->"json_metadata": "{\"tags\":[\"photographs\",\"status\",\"steemit\",\"tarazkp\"],\"image\":[\"https:\\/\\/cdn.steemitimages.com\\/DQmT1NNRC8imEH2kGUwGCJ6oBGt1DQxFfYXxHp3qkw3rCNg\\/IMG_0261.JPG\"],\"app\":\"steemit\\/0.1\",\"format\":\"markdown\"}",
->"post_id": 64760050

Response from test node at https://api.steemit.com:
->"reputation": 1609409893
->"json_metadata": "{\"format\":\"markdown\",\"app\":\"steemit\\/0.1\",\"tags\":[\"photographs\",\"status\",\"steemit\",\"tarazkp\"],\"image\":[\"https:\\/\\/cdn.steemitimages.com\\/DQmT1NNRC8imEH2kGUwGCJ6oBGt1DQxFfYXxHp3qkw3rCNg\\/IMG_0261.JPG\"]}",
->"post_id": 64720731

Note:
`"json_metadata"` differs only in order
Diffed with SVNTortoise as jsondiff broke with error

Linux call with additional parameter:
`python3 get_discussions_by_created.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"filter_tags":["tarazkp"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_created.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"filter_tags\":[\"tarazkp\"],\"truncate_body\":0}]"
Results:
`"filter_tags not supported"

#### get_discussions_by_feed
Linux call:
`python3 get_discussions_by_feed.py https://api.steem.house https://api.steemit.com ./ steemtools steempeak introducing-peaklock-and-keys-management 3

Windows call:
`python get_discussions_by_feed.py https://api.steemit.com https://api.steem.house ./ steemtools steempeak introducing-peaklock-and-keys-management 3

Results:
results are empty

Notes:
With
Linux call:
`python3 get_discussions_by_feed.py https://api.steem.house https://api.steemit.com ./ hk steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography 1

Windows call:
`python get_discussions_by_feed.py https://api.steemit.com https://api.steem.house ./ hk steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography 1

Results are:
```
Response from ref node at https://api.steem.house & https://api.steemit.com:
{
    "error": {
        "code": -32602,
        "data": "invalid account name length: `hk`",
        "message": "Invalid parameters"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```
changing tag to `landscapephotography` results to same error

#### get_discussions_by_hot
Linux call:
`python3 get_discussions_by_hot.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_hot.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
empty `[]

Linux call with additional parameter:
`python3 get_discussions_by_hot.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"filter_tags":["tarazkp"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_hot.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"filter_tags\":[\"tarazkp\"],\"truncate_body\":0}]"
Results:
`"filter_tags not supported"


#### get_discussions_by_promoted
Linux call:
`python3 get_discussions_by_promoted.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_promoted.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
empty `[]

Linux call with additional parameter:
`python3 get_discussions_by_promoted.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"filter_tags":["tarazkp"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_promoted.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"filter_tags\":[\"tarazkp\"],\"truncate_body\":0}]"
Results:
`"filter_tags not supported"

#### get_discussions_by_trending
Linux call:
`python3 get_discussions_by_trending.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_trending.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
empty `[]

Linux call with additional parameter:
`python3 get_discussions_by_trending.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"filter_tags":["tarazkp"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_trending.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"filter_tags\":[\"tarazkp\"],\"truncate_body\":0}]"
Results:
`"filter_tags not supported"


#### get_discussions_by_votes
Linux call:
`python3 get_discussions_by_trending.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_discussions_by_trending.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
empty `[]

Linux call with additional parameter:
`python3 get_discussions_by_trending.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"filter_tags":["tarazkp"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_discussions_by_trending.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"filter_tags\":[\"tarazkp\"],\"truncate_body\":0}]"
Results:
`"filter_tags not supported"

#### get_dynamic_global_properties
Linux call:
`python3 get_dynamic_global_properties.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_dynamic_global_properties.py https://api.steemit.com https://api.steem.house ./ 

Results:
At first look it works good but sometimes output differs in:
`current_aslot
`current_sbd_supply
`current_supply
`current_witness
`head_block_id
`head_block_number
`last_irreversible_block_num
`sps_interval_ledger
`time
`total_vesting_fund_steem
`total_vesting_shares
`virtual_supply

#### get_escrow
Linux call:
`python3 get_escrow.py https://api.steem.house https://api.steemit.com ./ steemit 1234

Windows call:
`python get_escrow.py https://api.steemit.com https://api.steem.house ./ steemit 1234

Results:
Result is null, according to documentation it's expected value

#### get_expiring_vesting_delegations
Linux call:
`python3 get_expiring_vesting_delegations.py https://api.steem.house https://api.steemit.com ./ steempeak 2019-01-01T00:00:00

Windows call:
`python get_expiring_vesting_delegations.py https://api.steemit.com https://api.steem.house ./ steempeak 2019-01-01T00:00:00

Results:
empty `[]

#### get_feed
Linux call:
`python3 get_feed.py https://api.steem.house https://api.steemit.com ./ steemit 0 1

Windows call:
`python get_feed.py https://api.steemit.com https://api.steem.house ./ steemit 0 1

Results:
`"Assert Exception:_follow_api: follow_api_plugin not enabled."

#### get_feed_entries
Linux call:
`python3 get_feed_entries.py https://api.steem.house https://api.steemit.com ./ steemit 0 1

Windows call:
`python get_feed_entries.py https://api.steemit.com https://api.steem.house ./ steemit 0 1

Results:
`"Assert Exception:_follow_api: follow_api_plugin not enabled."

#### get_feed_history
Linux call:
`python3 get_feed_history.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_feed_history.py https://api.steemit.com https://api.steem.house ./ 

Results:
Works fine

#### get_follow_count
Linux call:
`python3 get_follow_count.py https://api.steem.house https://api.steemit.com ./ steemmeupscotty

Windows call:
`python get_follow_count.py https://api.steemit.com https://api.steem.house ./ steemmeupscotty

Results:
Works good

#### get_followers
Linux call:
`python3 get_followers.py https://api.steem.house https://api.steemit.com ./ steemit null blog 10

Windows call:
`python get_followers.py https://api.steemit.com https://api.steem.house ./ steemit null blog 10

Results:
empty `[]

#### get_following
Linux call:
`python3 get_following.py https://api.steem.house https://api.steemit.com ./ steemit null blog 10

Windows call:
`python get_following.py https://api.steemit.com https://api.steem.house ./ steemit null blog 10

Results:
empty `[]

#### get_hardfork_version
Linux call:
`python3 get_hardfork_version.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_hardfork_version.py https://api.steemit.com https://api.steem.house ./ 

Results:
Looks good

#### get_key_references
Linux call:
`python3 get_key_references.py https://api.steem.house https://api.steemit.com ./ STM5jZtLoV8YbxCxr4imnbWn61zMB24wwonpnVhfXRmv7j6fk3dTH

Windows call:
`python get_key_references.py https://api.steemit.com https://api.steem.house ./ STM5jZtLoV8YbxCxr4imnbWn61zMB24wwonpnVhfXRmv7j6fk3dTH

Results:
`"Assert Exception:_account_by_key_api: account_history_api_plugin not enabled."

#### get_market_history
Linux call:
`python3 get_market_history.py https://api.steem.house https://api.steemit.com ./ 300 2018-01-01T00:00:00 2018-01-02T00:00:00

Windows call:
`python get_market_history.py https://api.steemit.com https://api.steem.house ./ 300 2018-01-01T00:00:00 2018-01-02T00:00:00

Results:
empty `[]
#### get_market_history_buckets
Linux call:
`python3 get_market_history_buckets.py https://api.steem.house https://api.steemit.com ./ 

Windows call:
`python get_market_history_buckets.py https://api.steemit.com https://api.steem.house ./

Results:
As expected
Works good

#### get_next_scheduled_hardfork
Linux call:
`python3 get_next_scheduled_hardfork.py https://api.steem.house https://api.steemit.com ./
Windows call:
`python get_next_scheduled_hardfork.py https://api.steemit.com https://api.steem.house ./
Results:
Works good

#### get_open_orders
Linux call:
`python3 get_open_orders.py https://api.steem.house https://api.steemit.com ./steemit
Windows call:
`python get_open_orders.py https://api.steemit.com https://api.steem.house ./ steemit

Results:
empty `[]

#### get_ops_in_block
Linux call:
`python3 get_ops_in_block.py https://api.steem.house https://api.steemit.com ./ 1
Windows call:
`python get_ops_in_block.py https://api.steemit.com https://api.steem.house ./ 1

Results:
looks good

#### get_order_book
Linux call:
`python3 get_order_book.py https://api.steem.house https://api.steemit.com ./ 18
Windows call:
`python get_order_book.py https://api.steemit.com https://api.steem.house ./ 18
Results:
looks good

#### get_owner_history
Linux call:
`python3 get_owner_history.py https://api.steem.house https://api.steemit.com ./ steemit
Windows call:
`python get_owner_history.py https://api.steemit.com https://api.steem.house ./ steemit 

Results:
looks good

#### get_post_discussions_by_payout
Linux call:
`python3 get_post_discussions_by_payout.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"truncate_body":0}]'
Windows call:
`python get_post_discussions_by_payout.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"truncate_body\":0}]"

Results:
empty `[]

Linux call with additional parameter:
`python3 get_post_discussions_by_payout.py https://api.steem.house https://api.steemit.com ./ '[{"tag":"tarazkp","limit":2,"filter_tags":["tarazkp"],"truncate_body":0}]'
Windows call with additional parameter:
`python get_post_discussions_by_payout.py https://api.steemit.com https://api.steem.house ./ "[{\"tag\":\"tarazkp\",\"limit\":2,\"filter_tags\":[\"tarazkp\"],\"truncate_body\":0}]"
Results:
`"get_post_discussions_by_payout() got an unexpected keyword argument 'filter_tags'",
`        "message": "Invalid parameters"

#### get_potential_signatures
Linux call:
`python3 get_potential_signatures.py https://api.steem.house https://api.steemit.com ./ '[{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["pow",{"worker_account":"cloop3","block_id":"00000449f7860b82b4fbe2f317c670e9f01d6d9a","nonce":3899,"work":{"worker":"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F","input":"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0","signature":"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a","work":"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"sbd_interest_rate":1000}}]],"extensions":[],"signatures":[]}]'
Windows call:
`python get_potential_signatures.py https://api.steemit.com https://api.steem.house ./ "[{\"ref_block_num\":1097,\"ref_block_prefix\":2181793527,\"expiration\":\"2016-03-24T18:00:21\",\"operations\":[[\"pow\",{\"worker_account\":\"cloop3\",\"block_id\":\"00000449f7860b82b4fbe2f317c670e9f01d6d9a\",\"nonce\":3899,\"work\":{\"worker\":\"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F\",\"input\":\"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0\",\"signature\":\"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a\",\"work\":\"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3\"},\"props\":{\"account_creation_fee\":{\"amount\":\"100000\",\"precision\":3,\"nai\":\"@@000000021\"},\"maximum_block_size\":131072,\"sbd_interest_rate\":1000}}]],\"extensions\":[],\"signatures\":[]}]"
Result:
empty `[]

#### get_reblogged_by
Linux call:
`python3 get_reblogged_by.py https://api.steem.house https://api.steemit.com ./ tarazkp the-legacy-of-yolo
Windows call:
`python get_reblogged_by.py https://api.steemit.com https://api.steem.house ./ tarazkp the-legacy-of-yolo
Results:
`"Assert Exception:_follow_api: follow_api_plugin not enabled."

#### get_recent_trades
Linux call:
`python3 get_recent_trades.py https://api.steem.house https://api.steemit.com ./ 986
Windows call:
`python get_recent_trades.py https://api.steemit.com https://api.steem.house ./ 986
Results:
looks fine

#### get_recovery_request
Linux call:
`python3 get_recovery_request.py https://api.steem.house https://api.steemit.com ./ steemit
Windows call:
`python get_recovery_request.py https://api.steemit.com https://api.steem.house ./ steemit
Results:
`null

#### get_replies_by_last_update
Linux call:
`python3 get_replies_by_last_update.py https://api.steem.house https://api.steemit.com ./ tarazkp the-legacy-of-yolo 10
Windows call:
`python get_replies_by_last_update.py https://api.steemit.com https://api.steem.house ./ tarazkp the-legacy-of-yolo 10
Results:
empty `[]

#### get_required_signatures
Linux call:
`python3 get_required_signatures.py https://api.steem.house https://api.steemit.com ./ '[{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["pow",{"worker_account":"cloop3","block_id":"00000449f7860b82b4fbe2f317c670e9f01d6d9a","nonce":3899,"work":{"worker":"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F","input":"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0","signature":"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a","work":"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"sbd_interest_rate":1000}}]],"extensions":[],"signatures":[]},[]]'
Windows call:
`python get_required_signatures.py https://api.steemit.com https://api.steem.house ./ "[{\"ref_block_num\":1097,\"ref_block_prefix\":2181793527,\"expiration\":\"2016-03-24T18:00:21\",\"operations\":[[\"pow\",{\"worker_account\":\"cloop3\",\"block_id\":\"00000449f7860b82b4fbe2f317c670e9f01d6d9a\",\"nonce\":3899,\"work\":{\"worker\":\"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F\",\"input\":\"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0\",\"signature\":\"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a\",\"work\":\"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3\"},\"props\":{\"account_creation_fee\":{\"amount\":\"100000\",\"precision\":3,\"nai\":\"@@000000021\"},\"maximum_block_size\":131072,\"sbd_interest_rate\":1000}}]],\"extensions\":[],\"signatures\":[]},[]]"
Results:
empty `[]

#### get_reward_fund
Linux call:
`python3 get_reward_fund.py https://api.steem.house https://api.steemit.com ./ post
Windows call:
`python get_reward_fund.py https://api.steemit.com https://api.steem.house ./ post
Results:
looks good

#### get_savings_withdraw_from
Linux call:
`python3 get_savings_withdraw_from.py https://api.steem.house https://api.steemit.com ./ steemit
Windows call:
`python get_savings_withdraw_from.py https://api.steemit.com https://api.steem.house ./ steemit
Results:
looks good

#### get_savings_withdraw_to
Linux call:
`python3 get_savings_withdraw_to.py https://api.steem.house https://api.steemit.com ./ steemit
Windows call:
`python get_savings_withdraw_to.py https://api.steemit.com https://api.steem.house ./ steemit
Results:
Looks fine

#### get_state
Deprecated

#### get_tags_used_by_author
Linux call:
`python3 get_tags_used_by_author.py https://api.steem.house https://api.steemit.com ./ steemit
Windows call:
`python get_tags_used_by_author.py https://api.steemit.com https://api.steem.house ./ steemit
Results:
`"Assert Exception:_tags_api: tags_api_plugin not enabled."

#### get_ticker
Linux call:
`python3 get_ticker.py https://api.steem.house https://api.steemit.com ./
Windows call:
`python get_ticker.py https://api.steemit.com https://api.steem.house ./
Results:
Looks good

#### get_trade_history
Linux call:
`python3 get_trade_history.py https://api.steem.house https://api.steemit.com ./ 2018-01-01T00:00:00 2018-01-02T00:00:00 10
Windows call:
`python get_trade_history.py https://api.steemit.com https://api.steem.house ./ 2018-01-01T00:00:00 2018-01-02T00:00:00 10
Results:
Looks good

#### get_transaction
Linux call:
`python3 get_transaction.py https://api.steem.house https://api.steemit.com ./ 6fde0190a97835ea6d9e651293e90c89911f933c
Windows call:
`python get_transaction.py https://api.steemit.com https://api.steem.house ./ 6fde0190a97835ea6d9e651293e90c89911f933c
Results:
`"_account_history_api: account_history_api_plugin not enabled."

#### get_transaction_hex
Linux call:
`python3 get_transaction_hex.py https://api.steem.house https://api.steemit.com ./ '[{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["pow",{"worker_account":"cloop3","block_id":"00000449f7860b82b4fbe2f317c670e9f01d6d9a","nonce":3899,"work":{"worker":"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F","input":"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0","signature":"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a","work":"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"sbd_interest_rate":1000}}]],"extensions":[],"signatures":[]}]'
Windows call:
`python get_transaction_hex.py https://api.steemit.com https://api.steem.house ./ "[{\"ref_block_num\":1097,\"ref_block_prefix\":2181793527,\"expiration\":\"2016-03-24T18:00:21\",\"operations\":[[\"pow\",{\"worker_account\":\"cloop3\",\"block_id\":\"00000449f7860b82b4fbe2f317c670e9f01d6d9a\",\"nonce\":3899,\"work\":{\"worker\":\"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F\",\"input\":\"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0\",\"signature\":\"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a\",\"work\":\"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3\"},\"props\":{\"account_creation_fee\":{\"amount\":\"100000\",\"precision\":3,\"nai\":\"@@000000021\"},\"maximum_block_size\":131072,\"sbd_interest_rate\":1000}}]],\"extensions\":[],\"signatures\":[]}]"
Results:
A long chain of unrelated characters, looks fine

#### get_trending_tags
Linux call:
`python3 get_trending_tags.py https://api.steem.house https://api.steemit.com ./ steem 10
Windows call:
`python get_trending_tags.py https://api.steemit.com https://api.steem.house ./ steem 10
Results:
Differences detected at: 
`comments
`top_posts
`total_payouts

#### get_version
Linux call:
`python3 get_version.py https://api.steem.house https://api.steemit.com ./
Windows call:
`python get_version.py https://api.steemit.com https://api.steem.house ./
Results:
I dont know if these are valid values
```
"blockchain_version": "0.22.1",
"fc_revision": "b0e0336500baac1d1f52dd883ca0b0dcb58d5623",
"steem_revision": "b0e0336500baac1d1f52dd883ca0b0dcb58d5623"
```

#### get_vesting_delegations
Linux call:
`python3 get_vesting_delegations.py https://api.steem.house https://api.steemit.com ./ tarazkp null 10
Windows call:
`python get_vesting_delegations.py https://api.steemit.com https://api.steem.house ./ tarazkp null 10
Results:
looks good

#### get_volume
Linux call:
`python3 get_volume.py https://api.steem.house https://api.steemit.com ./
Windows call:
`python get_volume.py https://api.steemit.com https://api.steem.house ./
Results:
looks good

#### get_withdraw_routes 
Linux call:
`python3 get_withdraw_routes.py https://api.steem.house https://api.steemit.com ./ tarazkp all
Windows call:
`python get_withdraw_routes.py https://api.steemit.com https://api.steem.house ./ tarazkp all
Results:
!!!!!! empty `[] 
According to documentation there should be any output

#### get_witness_by_account
###### Returns the witness of an account.
Example call:

`python get_witness_by_account.py https://api.steem.house https://api.steemit.com ./ blocktrades`

Result: no differences.

#### get_witness_count
###### Documentation is silent about this one. 
Example call:

`python get_witness_count.py https://api.steem.house https://api.steemit.com ./`

Result: no differences.

#### get_witness_schedule
###### Returns the current witness schedule. 
Example call:

`python get_witness_schedule.py https://api.steem.house https://api.steemit.com ./`

Result: no differences.

#### get_witnesses
###### Returns current witnesses.
Example call:

`python get_witnesses.py https://api.steem.house https://api.steemit.com ./ 0`

Result: no differences. I have no idea what this parameter is for (number).

#### get_witnesses_by_vote
###### Returns current witnesses by vote.
Example Linux call:

`python3 get_witnesses_by_vote.py https://api.steem.house https://api.steemit.com ./ '[""]' 10`

Example Windows call:

`python get_witnesses_by_vote.py https://api.steem.house https://api.steemit.com ./ "[\"\"]" 10`

Result: no differences.

#### list_proposal_votes
###### Returns all proposal votes, starting with the specified voter or proposal.id.
Example Linux call:

`python3 list_proposal_votes.py https://api.steem.house https://api.steemit.com ./ '[""]' 10 by_voter_proposal ascending active`

Example Windows call:

`python list_proposal_votes.py https://api.steem.house https://api.steemit.com ./ "[\"\"]" 10 by_voter_proposal ascending active`

Result: no differences.

#### list_proposals
###### Returns all proposals, starting with the specified creator or start date.
Example Linux call:

`python3 list_proposals.py https://api.steem.house https://api.steemit.com ./ '[""]' 10 by_creator ascending active`

Example Windows call:

`python list_proposals.py https://api.steem.house https://api.steemit.com ./ "[\"\"]" 10 by_creator ascending active`

Result: no differences.

#### lookup_account_names
###### Looks up account names. accounts:[string]
Example call:

`python lookup_account_names.py https://api.steemit.com https://api.steem.house ./ blocktrades block`

Result: no differences.

#### lookup_accounts
###### Looks up accounts starting with name. Parameters lower_bound_name:string; limit:int up to 1000
Example call:

`python lookup_accounts.py https://api.steemit.com https://api.steem.house ./ blocktrades 10`

Result: no differences.

#### lookup_witness_accounts
###### Looks up witness accounts starting with name. 
###### Parameters: lower_bound_name:string; limit:int up to 1000
Example call:

`python lookup_witness_accounts.py https://api.steemit.com https://api.steem.house ./ blocktrades 10`

```
{
    "id": 1,
    "jsonrpc": "2.0",
    "result": [
        "blocktrades",
        "blogchain",
        "bloggers",
        "bloggersclub",
        "blood",
        "bloomberg",
        "blue",
        "blue.cheese",
        "bluemist",
        "blueorgy"
    ]
}
```
Result: no differences, but shouldn't it return only accounts starting with "blocktrades"?

Example call 2: 

`python lookup_witness_accounts.py https://api.steemit.com https://api.steem.house ./ block 10`

```
{
    "id": 1,
    "jsonrpc": "2.0",
    "result": [
        "block-buster",
        "blockbrothers",
        "blockchain",
        "blockchainbunker",
        "blockchained",
        "blockchaininfo",
        "blockcores",
        "blockstream",
        "blocktech",
        "blocktrades"
    ]
}
```
Result: no differences, returns accounts starting with "block" (as expected).

#### verify_account_authority
###### Not implemented.

#### verify_authority 
##### Returns true if the transaction has all of the required signatures.

Example Linux call:

`python3 verify_authority.py https://api.steem.house https://api.steemit.com ./ 
'{"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation",
"value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]}'`

Example Windows call:

`python verify_authority.py https://api.steem.house https://api.steemit.com ./ "{\"ref_block_num\":3738,\"ref_block_prefix\":1233832719,\"expiration\":\"2020-03-11T08:48:57\",\"operations\":[{\"type\":\"vote_operation\",\"value\":{\"voter\":\"anthrovegan\",\"author\":\"carrinm\",\"permlink\":\"actifit-carrinm-20200311t080841657z\",\"weight\":5000}}],\"extensions\":[],\"signatures\":[\"1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08\"]}"`

Result: no differences.