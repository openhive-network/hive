# database_api test notes
Test will follow commands listed in `https://developers.steem.io/apidefinitions/`

## find_account_recovery_requests 
`python3 find_account_recovery_requests.py https://api.steem.house https://api.steemit.com ./ steemit`

Expected result: empty result

## find_accounts 
`python3 find_accounts.py https://api.steem.house https://api.steemit.com ./ steemit alice`

## find_change_recovery_account_requests 
`python3 find_change_recovery_account_requests.py https://api.steem.house https://api.steemit.com ./ steemit alice`

Expected result: empty result

## find_comments
`python3 find_comments.py https://api.steem.house https://api.steemit.com ./`

Expected result: empty result

## find_decline_voting_rights_requests 
`python3 find_decline_voting_rights_requests.py https://api.steem.house https://api.steemit.com ./ steemit`

Expected result: empty result

## find_escrows 
`python3 find_escrows.py https://api.steem.house https://api.steemit.com ./ temp`

## find_limit_orders 
`python3 find_limit_orders.py https://api.steem.house https://api.steemit.com ./ temp`

Expected result: empty result

## find_owner_histories 
`python3 find_owner_histories.py https://api.steem.house https://api.steemit.com ./ steemit`

Expected result: empty result

## find_proposals 
`python3 find_proposals.py https://api.steem.house https://api.steemit.com ./ 0 23`

## find_savings_withdrawals 
`python3 find_savings_withdrawals.py https://api.steem.house https://api.steemit.com ./ steemit`

## find_hbd_conversion_requests 
`python3 find_hbd_conversion_requests.py https://api.steem.house https://api.steemit.com ./ steemit`

## find_smt_contributions 
`python3 find_smt_contributions.py https://api.steem.house https://api.steemit.com ./`

From HF23!

## find_smt_token_emissions
`python3 find_smt_token_emissions.py https://api.steem.house https://api.steemit.com ./ @@422838704 0`

From HF23!

## find_smt_tokens 
`python3 find_smt_tokens.py https://api.steem.house https://api.steemit.com ./ false`

From HF23!

## find_vesting_delegation_expirations 
`python3 find_vesting_delegation_expirations.py https://api.steem.house https://api.steemit.com ./ steem`

## find_vesting_delegations 
`python3 find_vesting_delegations.py https://api.steem.house https://api.steemit.com ./ steem`

## find_votes 
`python3 find_votes.py https://api.steem.house https://api.steemit.com ./ steemit firstpost`

## find_withdraw_vesting_routes 
`python3 find_withdraw_vesting_routes.py https://api.steem.house https://api.steemit.com ./ steem by_destination`

Expected result: empty result

## find_witnesses
`python3 find_witnesses.py https://api.steem.house https://api.steemit.com ./ blocktrades`

## get_active_witnesses 
`python3 get_active_witnesses.py https://api.steem.house https://api.steemit.com ./`

## get_config 
`python3 get_config.py https://api.steem.house https://api.steemit.com ./`

## get_current_price_feed 
`python3 get_current_price_feed.py https://api.steem.house https://api.steemit.com ./`

## get_dynamic_global_properties 
`python3 get_dynamic_global_properties.py https://api.steem.house https://api.steemit.com ./`

## get_feed_history 
`python3 get_feed_history.py https://api.steem.house https://api.steemit.com ./`

## get_hardfork_properties 
`python3 get_hardfork_properties.py https://api.steem.house https://api.steemit.com ./`

## get_nai_pool 
`python3 get_nai_pool.py https://api.steem.house https://api.steemit.com ./`

From HF23!

## get_order_book 
`python3 get_order_book.py https://api.steem.house https://api.steemit.com ./ 10`

## get_potential_signatures 
`python3 get_potential_signatures.py https://api.steem.house https://api.steemit.com ./ '{"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation", "value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]}'`

## get_required_signatures
`python3 get_required_signatures.py https://api.steem.house https://api.steemit.com ./ '{"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation", "value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]}' STM5HCCNJ6wnvePAoB2JhpbSgEfNcVR3w6EguhhopGV5Vgeck7Whh STM8TLiWw92GWEzy6qhJomJy8CzWSAFzCmCqsUQba6f1npNT7ouRA STM6gaC6PJBUHNg1NE56NwLz3EdeMdWRbuM7tsujMYgc24XELPk83`

## get_reward_funds 
`python3 get_reward_funds.py https://api.steem.house https://api.steemit.com ./`

## get_transaction_hex
`python3 get_transaction_hex.py https://api.steem.house https://api.steemit.com ./ '{"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation", "value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]}'`

## get_version 
`python3 get_version.py https://api.steem.house https://api.steemit.com ./`

## get_witness_schedule 
`python3 get_witness_schedule.py https://api.steem.house https://api.steemit.com ./`

## list_account_recovery_requests 
`python3 list_account_recovery_requests.py https://api.steem.house https://api.steemit.com ./ '""' 10 by_account`

Expected result: empty result

## list_accounts 
`python3 list_accounts.py https://api.steem.house https://api.steemit.com ./ '""' 1 by_name`

## list_change_recovery_account_requests 
`python3 list_change_recovery_account_requests.py https://api.steem.house https://api.steemit.com ./ '"alice"' 1 by_account`

## list_comments 
`python3 list_comments.py https://api.steem.house https://api.steemit.com ./ '["steemit","firstpost","",""]' 1 by_root`

## list_decline_voting_rights_requests 
`python3 list_decline_voting_rights_requests.py https://api.steem.house https://api.steemit.com ./ '""' 10 by_account`

## list_escrows 
`python3 list_escrows.py https://api.steem.house https://api.steemit.com ./ '["",0]' 10 by_from_id`

## list_limit_orders 
`python3 list_limit_orders.py https://api.steem.house https://api.steemit.com ./ '["alice",0]' 10 by_account`

## list_owner_histories 
`python3 list_owner_histories.py https://api.steem.house https://api.steemit.com ./ '["alice","1970-01-01T00:00:00"]' 10`

## list_proposal_votes 
`python3 list_proposal_votes.py https://api.steem.house https://api.steemit.com ./ '[""]' 10 by_voter_proposal ascending active`

## list_proposals 
`python3 list_proposals.py https://api.steem.house https://api.steemit.com ./ '[""]' 10 by_creator ascending active`

## list_savings_withdrawals 
`python3 list_savings_withdrawals.py https://api.steem.house https://api.steemit.com ./ '[0]' 10 by_from_id`

## list_hbd_conversion_requests 
`python3 list_hbd_conversion_requests.py https://api.steem.house https://api.steemit.com ./ '["steemit", 0]' 10 by_account`

## list_smt_contributions 
`python3 list_smt_contributions.py https://api.steem.house https://api.steemit.com ./ '[{"nai": "@@422838704", "decimals": 0}, 0]' 10 by_symbol_id`

From HF23!

## list_smt_token_emissions 
`python3 list_smt_token_emissions.py https://api.steem.house https://api.steemit.com ./ '[{"nai": "@@422838704", "decimals": 0}, "2019-08-07T16:54:03"]' 10 by_symbol_time`

From HF23!

## list_smt_tokens 
`python3 list_smt_tokens.py https://api.steem.house https://api.steemit.com ./ '{"nai": "@@422838704", "decimals": 0}' 10 by_symbol`

From HF23!

## list_vesting_delegation_expirations 
`python3 list_vesting_delegation_expirations.py https://api.steem.house https://api.steemit.com ./ '["1970-01-01T00:00:00",0]' 10 by_expiration`

## list_vesting_delegations 
`python3 list_vesting_delegations.py https://api.steem.house https://api.steemit.com ./ '["",""]' 10 by_delegation`

## list_votes 
`python3 list_votes.py https://api.steem.house https://api.steemit.com ./ '["","",""]' 10 by_comment_voter`

## list_withdraw_vesting_routes 
`python3 list_withdraw_vesting_routes.py https://api.steem.house https://api.steemit.com ./ '["",0]' 10 by_destination`

## list_witness_votes 
`python3 list_witness_votes.py https://api.steem.house https://api.steemit.com ./ '["",""]' 10 by_account_witness`

## list_witnesses 
`python3 list_witnesses.py https://api.steem.house https://api.steemit.com ./ '""' 10 by_name`

## verify_account_authority 
`python3 verify_account_authority.py https://api.steem.house https://api.steemit.com ./ temp STM8GC13uCZbP44HzMLV6zPZGwVQ8Nt4Kji8PapsPiNq1BK153XTX`

## verify_authority 
`python3 verify_authority.py https://api.steem.house https://api.steemit.com ./ '{"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation", "value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]}'`

## verify_signatures 
`python3 verify_signatures.py https://api.steem.house https://api.steemit.com ./ '{"hash": "0000000000000000000000000000000000000000000000000000000000000000", "signatures": [], "required_owner": [], "required_active": [], "required_posting": [], "required_other": []}'`
