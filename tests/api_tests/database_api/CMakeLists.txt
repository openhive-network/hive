# Adding python tests to cmake
MESSAGE( STATUS "### Adding database_api python tests")

## find_account_recovery_requests
ADD_API_TEST(database_api find_account_recovery_requests steemit)

## find_accounts
ADD_API_TEST(database_api find_accounts steemit alice)

## find_change_recovery_account_requests
ADD_API_TEST(database_api find_change_recovery_account_requests steemit alice)

## find_comments
ADD_API_TEST(database_api find_comments)

## find_decline_voting_rights_requests
ADD_API_TEST(database_api find_decline_voting_rights_requests steemit)

## find_escrows
ADD_API_TEST(database_api find_escrows temp)

## find_limit_orders
ADD_API_TEST(database_api find_limit_orders temp)

## find_owner_histories
ADD_API_TEST(database_api find_owner_histories steemit)

## find_proposals
ADD_API_TEST(database_api find_proposals 0 23)

## find_savings_withdrawals
ADD_API_TEST(database_api find_savings_withdrawals steemit)

## find_sbd_conversion_requests
ADD_API_TEST(database_api find_sbd_conversion_requests steemit)

## find_smt_contributions
# From HF23!
# ADD_API_TEST(database_api find_smt_contributions)

## find_smt_token_emissions
# From HF23!
#ADD_API_TEST(database_api find_smt_token_emissions @@422838704 0)

## find_smt_tokens
# From HF23!
#ADD_API_TEST(database_api find_smt_tokens false)

## find_vesting_delegation_expirations 
ADD_API_TEST(database_api find_vesting_delegation_expirations steem)

## find_vesting_delegations 
ADD_API_TEST(database_api find_vesting_delegations steem)

## find_votes 
ADD_API_TEST(database_api find_votes steemit firstpost)

## find_withdraw_vesting_routes 
ADD_API_TEST(database_api find_withdraw_vesting_routes steem by_destination)

## find_witnesses
ADD_API_TEST(database_api find_witnesses blocktrades)

## get_active_witnesses 
ADD_API_TEST(database_api get_active_witnesses)

## get_config 
ADD_API_TEST(database_api get_config)

## get_current_price_feed 
ADD_API_TEST(database_api get_current_price_feed)

## get_dynamic_global_properties 
ADD_API_TEST(database_api get_dynamic_global_properties)

## get_feed_history 
ADD_API_TEST(database_api get_feed_history)

## get_hardfork_properties 
ADD_API_TEST(database_api get_hardfork_properties)

## get_nai_pool 
# From HF23!
ADD_API_TEST(database_api get_nai_pool)

## get_order_book 
ADD_API_TEST(database_api get_order_book 10)

## get_potential_signatures 
ADD_API_TEST(database_api get_potential_signatures {"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation","value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]})

## get_required_signatures
ADD_API_TEST(database_api get_required_signatures {"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation","value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]} STM5HCCNJ6wnvePAoB2JhpbSgEfNcVR3w6EguhhopGV5Vgeck7Whh STM8TLiWw92GWEzy6qhJomJy8CzWSAFzCmCqsUQba6f1npNT7ouRA STM6gaC6PJBUHNg1NE56NwLz3EdeMdWRbuM7tsujMYgc24XELPk83)

## get_reward_funds 
ADD_API_TEST(database_api get_reward_funds)

## get_transaction_hex
ADD_API_TEST(database_api get_transaction_hex {"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation","value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]})

## get_version 
ADD_API_TEST(database_api get_version)

## get_witness_schedule 
ADD_API_TEST(database_api get_witness_schedule)

## list_account_recovery_requests 
ADD_API_TEST(database_api list_account_recovery_requests \"\" 10 by_account)

## list_accounts 
ADD_API_TEST(database_api list_accounts \"\" 1 by_name)

## list_change_recovery_account_requests 
ADD_API_TEST(database_api list_change_recovery_account_requests "alice" 1 by_account)

## list_comments 
ADD_API_TEST(database_api list_comments ["steemit","firstpost","",""] 1 by_root)

## list_decline_voting_rights_requests 
ADD_API_TEST(database_api list_decline_voting_rights_requests \"\" 10 by_account)

## list_escrows 
ADD_API_TEST(database_api list_escrows ["",0] 10 by_from_id)

## list_limit_orders 
ADD_API_TEST(database_api list_limit_orders ["alice",0] 10 by_account)

## list_owner_histories 
ADD_API_TEST(database_api list_owner_histories ["alice","1970-01-01T00:00:00"] 10)

## list_proposal_votes 
ADD_API_TEST(database_api list_proposal_votes [""] 10 by_voter_proposal ascending active)

## list_proposals 
ADD_API_TEST(database_api list_proposals [""] 10 by_creator ascending active)

## list_savings_withdrawals 
ADD_API_TEST(database_api list_savings_withdrawals [0] 10 by_from_id)

## list_sbd_conversion_requests 
ADD_API_TEST(database_api list_sbd_conversion_requests ["steemit",0] 10 by_account)

## list_smt_contributions 
# From HF23!
# ADD_API_TEST(database_api list_smt_contributions '[{"nai": "@@422838704", "decimals": 0}, 0]' 10 by_symbol_id)

## list_smt_token_emissions 
# From HF23!
#ADD_API_TEST(database_api list_smt_token_emissions '[{"nai": "@@422838704", "decimals": 0}, "2019-08-07T16:54:03"]' 10 by_symbol_time)

## list_smt_tokens 
#From HF23!
#ADD_API_TEST(database_api list_smt_tokens '{"nai": "@@422838704", "decimals": 0}' 10 by_symbol)

## list_vesting_delegation_expirations 
ADD_API_TEST(database_api list_vesting_delegation_expirations ["1970-01-01T00:00:00",0] 10 by_expiration)

## list_vesting_delegations 
ADD_API_TEST(database_api list_vesting_delegations ["",""] 10 by_delegation)

## list_votes 
ADD_API_TEST(database_api list_votes ["","",""] 10 by_comment_voter)

## list_withdraw_vesting_routes 
ADD_API_TEST(database_api list_withdraw_vesting_routes ["",0] 10 by_destination)

## list_witness_votes 
ADD_API_TEST(database_api list_witness_votes ["",""] 10 by_account_witness)

## list_witnesses 
ADD_API_TEST(database_api list_witnesses \"\" 10 by_name)

## verify_account_authority 
ADD_API_TEST(database_api verify_account_authority temp STM8GC13uCZbP44HzMLV6zPZGwVQ8Nt4Kji8PapsPiNq1BK153XTX)

## verify_authority 
ADD_API_TEST(database_api verify_authority {"ref_block_num":3738,"ref_block_prefix":1233832719,"expiration":"2020-03-11T08:48:57","operations":[{"type":"vote_operation","value":{"voter":"anthrovegan","author":"carrinm","permlink":"actifit-carrinm-20200311t080841657z","weight":5000}}],"extensions":[],"signatures":["1f4efcb6c7efe9001de42a4f072493de10b8bfd576b3b647532c7facc3d0580a7209544e49a97e7d74124aae86e6012a1f4cd33bf5ca16620bad7077364f820c08"]})

## verify_signatures 
ADD_API_TEST(database_api verify_signatures {"hash":"0000000000000000000000000000000000000000000000000000000000000000","signatures":[],"required_owner":[],"required_active":[],"required_posting":[],"required_other":[]})

