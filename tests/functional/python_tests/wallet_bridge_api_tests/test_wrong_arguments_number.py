import pytest

from test_tools import exceptions

# TODO BUG LIST!
"""
1. Problem with run command wallet_bridge_api.find_proposals() with additional argument. Method return exception:
   "Parse Error:Couldn't parse uint64_t"    (# BUG1)
   
2. Problem with run command wallet_bridge_api.get_account() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): account name is required as first argument"   (# BUG2)
   
3. Problem with run command wallet_bridge_api.get_block() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_numeric(): block number is required as first argument"    (# BUG3)
   
4. Problem with run command wallet_bridge_api.get_open_orders() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): account name is required as first argument"     (# BUG4)
   
5. Problem with run command wallet_bridge_api.get_owner_history() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): Account name is required as first argument."    (# BUG5)
   
6. Problem with run command wallet_bridge_api.is_known_transaction() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): Transaction id is required"    (# BUG6)
   
7. Problem with run command wallet_bridge_api.get_reward_fund() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): Reward fund name is required as first argument"   (# BUG7)
   
8. Problem with run command wallet_bridge_api.get_order_book() with additional argument. Method return exception:
   "Assert Exception:args.get_array()[0].is_numeric(): Orders limit is required as first argument"   (# BUG8)
"""

# TODO REMOVE THIS LIST AFTER REVIEW
# COMMANDS_WITH_CORRECT_ARGUMENTS = [
#     ('list_proposals', ([""], 100, 29, 0, 0)),
#     ('list_proposal_votes', ([""], 100, 33, 0, 0)),
#     ('find_proposals', ([0])),
#     ('get_active_witnesses', ()),
#     ('get_chain_properties', ()),
#     ('get_feed_history', ()),
#     ('get_current_median_history_price', ()),
#     ('get_dynamic_global_properties', ()),
#     ('get_hardfork_version', ()),
#     ('get_witness_schedule', ()),
#     ('find_rc_accounts', ([''])),
#     ('list_rc_accounts', ('initminer', 100)),
#     ('list_rc_direct_delegations', (['initminer', ''], 100)),
#     ('get_account', ('get_account')),
#     ('get_account_history', ('non-exist-acc', -1, 1000)),
#     ('get_accounts', (['non-exist-acc'])),
#     ('get_block', 0),
#     ('get_ops_in_block', (0, True)),
#     ('get_open_orders', ('non-exist-acc')),
#     ('get_owner_history', ('non-exist-acc')),
#     ('is_known_transaction', '0000000000000000000000000000000000000000'),
#     ('get_reward_fund', 'post'),
# ]


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'broadcast_transaction',
        'broadcast_transaction_synchronous',
        'find_proposals',
        'find_rc_accounts',
        'find_recurrent_transfers',
        'get_account',
        'get_account_history',
        'get_accounts',
        'get_block',
        'get_collateralized_conversion_requests',
        'get_conversion_requests',
        'get_open_orders',
        'get_ops_in_block',
        'get_order_book',
        'get_owner_history',
        'get_reward_fund',
        'get_transaction',
        'get_withdraw_routes',
        'get_witness',
        'is_known_transaction',
        'list_accounts',
        'list_my_accounts',
        'list_proposal_votes',
        'list_proposals',
        'list_rc_accounts',
        'list_rc_direct_delegations',
        'list_witnesses',
    ]
)
def test_run_command_without_arguments_where_arguments_are_required(node, wallet_bridge_api_command):
    # TODO In final version use 'COMMANDS_WITH_CORRECT_ARGUMENTS'
    with pytest.raises(exceptions.CommunicationError):
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()


@pytest.mark.parametrize(
    'wallet_bridge_api_command, arguments', [
        ('list_proposals', ([""], 100, 29, 0, 0)),
        ('list_proposal_votes', ([""], 100, 33, 0, 0)),
        # ('find_proposals', ([0])),   # BUG1
        ('get_active_witnesses', ()),
        ('get_chain_properties', ()),
        ('get_feed_history', ()),
        ('get_current_median_history_price', ()),
        ('get_dynamic_global_properties', ()),
        ('get_hardfork_version', ()),
        ('get_witness_schedule', ()),
        ('find_rc_accounts', ([''])),
        ('list_rc_accounts', ('initminer', 100)),
        ('list_rc_direct_delegations', (['initminer', ''], 100)),
        # ('get_account', ('get_account')),  # BUG2
        ('get_account_history', ('non-exist-acc', -1, 1000)),
        ('get_accounts', (['non-exist-acc'])),
        # ('get_block', 0),  # BUG3
        ('get_ops_in_block', (0, True)),
        # ('get_open_orders', ('non-exist-acc')),  # BUG4
        # ('get_owner_history', ('non-exist-acc')),   # BUG5
        # ('is_known_transaction', '0000000000000000000000000000000000000000'),  # BUG6
        # ('get_reward_fund', 'post'), # BUG7
        # ('get_order_book', 10),  # BUG8
    ]
)
def test_run_command_with_additional_argument(node, wallet_bridge_api_command, arguments):
    #'list_my_accounts' is testing in file test_accounts.py
    #'get_withdraw_routes' is testing in file test_other_commands.py
    #'get_transaction' is testing in file test_other_commands.py
    #'get_conversion_requests' is testing in file test_other_commands.py
    #'get_collateralized_conversion_requests' is testing in file test_other_commands.py
    #'find_recurrent_transfers' is testing in file test_other_commands.py
    #'broadcast_transaction' is testing in file test_other_commands.py
    #'broadcast_transaction_synchronous' is testing in file test_other_commands.py


    if type(arguments) is int:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)(arguments, 'additional_string_argument')
    else:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments, 'additional_string_argument')


@pytest.mark.parametrize(
    'wallet_bridge_api_command, arguments', [
        ('list_proposals', ([""], 100, 29, 0, 0)),
        ('list_proposal_votes', ([""], 100, 33, 0, 0)),
        ('find_proposals', ([0])),
        ('get_active_witnesses', ()),
        ('get_chain_properties', ()),
        ('get_feed_history', ()),
        ('get_current_median_history_price', ()),
        ('get_dynamic_global_properties', ()),
        ('get_hardfork_version', ()),
        ('get_witness_schedule', ()),
        ('find_rc_accounts', ([''])),
        ('list_rc_accounts', ('initminer', 100)),
        ('list_rc_direct_delegations', (['initminer', ''], 100)),
        ('get_account', ('get_account')),
        ('get_account_history', ('non-exist-acc', -1, 1000)),
        ('get_accounts', (['non-exist-acc'])),
        ('get_block', 0),
        ('get_ops_in_block', (0, True)),
        ('get_open_orders', ('non-exist-acc')),
        ('get_owner_history', ('non-exist-acc')),
        ('is_known_transaction', '0000000000000000000000000000000000000000'),
        ('get_reward_fund', 'post'),
        ('get_transaction', 'transaction_id'),
        ('get_conversion_requests', 'alice'),
        ('get_collateralized_conversion_requests', 'alice'),
        ('find_recurrent_transfers', 'alice'),
        ('broadcast_transaction', 'transaction'),
        ('broadcast_transaction_synchronous', 'transaction'),
        ('get_order_book', 10),
    ]
)
def test_run_command_with_missing_argument(node, wallet_bridge_api_command, arguments):
    #'list_my_accounts' is testing in file test_accounts.py
    #'get_withdraw_routes' is testing in file test_other_commands.py
    if type(arguments) is tuple and len(arguments) >= 1:
        with pytest.raises(exceptions.CommunicationError):
            getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments[:-1])
    elif type(arguments) is not tuple:
        with pytest.raises(exceptions.CommunicationError):
            getattr(node.api.wallet_bridge, wallet_bridge_api_command)()
