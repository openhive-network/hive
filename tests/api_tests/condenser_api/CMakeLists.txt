# Adding python tests to cmake
MESSAGE( STATUS "### Adding condenser_api python tests")
## broadcast_block 
ADD_API_TEST(condenser_api broadcast_block [{"previous":"0000000000000000000000000000000000000000","timestamp":"1970-01-01T00:00:00","witness":"","transaction_merkle_root":"0000000000000000000000000000000000000000","extensions":[],"witness_signature":"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","transactions":[]}])

## broadcast_transaction
ADD_API_TEST(condenser_api broadcast_transaction [{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["vote",{"voter":"steemit","author":"alice","permlink":"a-post-by-alice","weight":10000}]],"extensions":[],"signatures":[]}])

## broadcast_transaction_synchronous
ADD_API_TEST(condenser_api broadcast_transaction_synchronous [{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["vote",{"voter":"steemit","author":"alice","permlink":"a-post-by-alice","weight":10000}]],"extensions":[],"signatures":[]}])

## find_proposals
ADD_API_TEST(condenser_api find_proposals 0 23)

## get_account_bandwidth
#Its disabled since 0.20.6 https://developers.steem.io/apidefinitions/#condenser_api.get_account_bandwidth

## get_account_count
ADD_API_TEST(condenser_api get_account_count)

## get_account_history
ADD_API_TEST(condenser_api get_account_history steemit -1 10)

## get_account_references
#Not implemented https://developers.steem.io/apidefinitions/#condenser_api.get_account_references

## get_account_reputations
ADD_API_TEST(condenser_api get_account_reputations steemit 10)

## get_account_votes
ADD_API_TEST(condenser_api get_account_votes alice)

## get_accounts
ADD_API_TEST(condenser_api get_accounts steemit alice steempeak)

## get_active_votes
ADD_API_TEST(condenser_api get_active_votes drakos open-letter-to-justin-sun-and-the-steem-community)

## get_active_witnesses
ADD_API_TEST(condenser_api get_active_witnesses)

## get_block
ADD_API_TEST(condenser_api get_block 1234)

## get_block_header
ADD_API_TEST(condenser_api get_block_header 123454)

## get_blog
ADD_API_TEST(condenser_api get_blog drakos 0 1)

## get_blog_authors
ADD_API_TEST(condenser_api get_blog_authors drakos)

## get_blog_entries
ADD_API_TEST(condenser_api get_blog_entries tarazkp 0 10)

## get_chain_properties
ADD_API_TEST(condenser_api get_chain_properties)

## get_comment_discussions_by_payout!
ADD_API_TEST(condenser_api get_comment_discussions_by_payout [{"tag":"photography","limit":10,"truncate_body":0}])

## get_config
ADD_API_TEST(condenser_api get_config)

## get_content
ADD_API_TEST(condenser_api get_content tarazkp the-legacy-of-yolo)

## get_content_replies
ADD_API_TEST(condenser_api get_content_replies tarazkp the-legacy-of-yolo)

## get_conversion_requests
ADD_API_TEST(condenser_api get_conversion_requests 1232345)

## get_current_median_history_price
ADD_API_TEST(condenser_api get_current_median_history_price)

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

## get_dynamic_global_properties
ADD_API_TEST(condenser_api get_dynamic_global_properties)

## get_escrow
ADD_API_TEST(condenser_api get_escrow steemit 1234)

## get_expiring_vesting_delegations
ADD_API_TEST(condenser_api get_expiring_vesting_delegations steempeak 2019-01-01T00:00:00)

## get_feed
ADD_API_TEST(condenser_api get_feed steemit 0 1)

## get_feed_entries
ADD_API_TEST(condenser_api get_feed_entries steemit 0 1)

## get_feed_history
ADD_API_TEST(condenser_api get_feed_history)

## get_follow_count
ADD_API_TEST(condenser_api get_follow_count steemmeupscotty)

## get_followers
ADD_API_TEST(condenser_api get_followers steemit null blog 10)

## get_following
ADD_API_TEST(condenser_api get_following steemit null blog 10)

## get_hardfork_version
ADD_API_TEST(condenser_api get_hardfork_version)

## get_key_references
ADD_API_TEST(condenser_api get_key_references STM5jZtLoV8YbxCxr4imnbWn61zMB24wwonpnVhfXRmv7j6fk3dTH)

## get_market_history
ADD_API_TEST(condenser_api get_market_history 300 2018-01-01T00:00:00 2018-01-02T00:00:00)

## get_market_history_buckets
ADD_API_TEST(condenser_api get_market_history_buckets)

## get_next_scheduled_hardfork
ADD_API_TEST(condenser_api get_next_scheduled_hardfork)

## get_open_orders
ADD_API_TEST(condenser_api get_open_orders steemit)

## get_ops_in_block
ADD_API_TEST(condenser_api get_ops_in_block 1)

## get_order_book
ADD_API_TEST(condenser_api get_order_book 18)

## get_owner_history
ADD_API_TEST(condenser_api get_owner_history steemit)

## get_post_discussions_by_payout
ADD_API_TEST(condenser_api get_post_discussions_by_payout [{"tag":"tarazkp","limit":2,"truncate_body":0}])

## get_potential_signatures
ADD_API_TEST(condenser_api get_potential_signatures [{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["pow",{"worker_account":"cloop3","block_id":"00000449f7860b82b4fbe2f317c670e9f01d6d9a","nonce":3899,"work":{"worker":"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F","input":"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0","signature":"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a","work":"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}]],"extensions":[],"signatures":[]}])

## get_reblogged_by
ADD_API_TEST(condenser_api get_reblogged_by tarazkp the-legacy-of-yolo)

## get_recent_trades
ADD_API_TEST(condenser_api get_recent_trades 10)

## get_recovery_request
ADD_API_TEST(condenser_api get_recovery_request steemit)

## get_replies_by_last_update
ADD_API_TEST(condenser_api get_replies_by_last_update tarazkp the-legacy-of-yolo 10)

## get_required_signatures
ADD_API_TEST(condenser_api get_required_signatures [{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["pow",{"worker_account":"cloop3","block_id":"00000449f7860b82b4fbe2f317c670e9f01d6d9a","nonce":3899,"work":{"worker":"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F","input":"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0","signature":"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a","work":"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}]],"extensions":[],"signatures":[]},[]])

## get_reward_fund
ADD_API_TEST(condenser_api get_reward_fund post)

## get_savings_withdraw_from
ADD_API_TEST(condenser_api get_savings_withdraw_from steemit)

## get_savings_withdraw_to
ADD_API_TEST(condenser_api get_savings_withdraw_to steemit)

## get_state
# Deprecated

## get_tags_used_by_author
ADD_API_TEST(condenser_api get_tags_used_by_author steemit)

## get_ticker
ADD_API_TEST(condenser_api get_ticker)

## get_trade_history
ADD_API_TEST(condenser_api get_trade_history 2018-01-01T00:00:00 2018-01-02T00:00:00 10)

## get_transaction
ADD_API_TEST(condenser_api get_transaction 6fde0190a97835ea6d9e651293e90c89911f933c)

## get_transaction_hex
ADD_API_TEST(condenser_api get_transaction_hex [{"ref_block_num":1097,"ref_block_prefix":2181793527,"expiration":"2016-03-24T18:00:21","operations":[["pow",{"worker_account":"cloop3","block_id":"00000449f7860b82b4fbe2f317c670e9f01d6d9a","nonce":3899,"work":{"worker":"STM7P5TDnA87Pj9T4mf6YHrhzjC1KbPZpNxLWCcVcHxNYXakpoT4F","input":"ae8e7c677119d22385f8c48026fee7aad7bba693bf788d7f27047f40b47738c0","signature":"1f38fe9a3f9989f84bd94aa5bbc88beaf09b67f825aa4450cf5105d111149ba6db560b582c7dbb026c7fc9c2eb5051815a72b17f6896ed59d3851d9a0f9883ca7a","work":"000e7b209d58f2e64b36e9bf12b999c6c7af168cc3fc41eb7f8a4bf796c174c3"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}]],"extensions":[],"signatures":[]}])

## get_trending_tags
ADD_API_TEST(condenser_api get_trending_tags blocktrades 1)

## get_version
ADD_API_TEST(condenser_api get_version)

## get_vesting_delegations
ADD_API_TEST(condenser_api get_vesting_delegations tarazkp null 10)

## get_volume
ADD_API_TEST(condenser_api get_volume)

## get_withdraw_routes 
ADD_API_TEST(condenser_api get_withdraw_routes tarazkp all)

## get_witness_by_account
ADD_API_TEST(condenser_api get_witness_by_account blocktrades)

## get_witness_count
ADD_API_TEST(condenser_api get_witness_count)

## get_witness_schedule
ADD_API_TEST(condenser_api get_witness_schedule)

## get_witnesses
ADD_API_TEST(condenser_api get_witnesses 0)

## get_witnesses_by_vote
ADD_API_TEST(condenser_api get_witnesses_by_vote [""] 10)

## list_proposal_votes
ADD_API_TEST(condenser_api list_proposal_votes [""] 10 by_voter_proposal ascending active)

## list_proposals
ADD_API_TEST(condenser_api list_proposals [""] 10 by_creator ascending active)

## lookup_account_names
ADD_API_TEST(condenser_api lookup_account_names blocktrades block)

## lookup_accounts
ADD_API_TEST(condenser_api lookup_accounts blocktrades 10)

## lookup_witness_accounts
ADD_API_TEST(condenser_api lookup_witness_accounts blocktrades 10)

## verify_account_authority
# Not implemented.

## verify_authority 
ADD_API_TEST(condenser_api verify_authority {"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation","value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]})
