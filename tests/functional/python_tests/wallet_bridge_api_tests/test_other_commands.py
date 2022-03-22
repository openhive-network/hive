import pytest

from test_tools import Asset, exceptions
from validate_message import validate_message
import schemas

# TODO BUG LIST!
"""
1. Problem with running wallet_bridge_api.get_block with incorrect argument (putting negative value as 
   argument). I think, in this situation program should throw an exception. (# BUG1) 

2. Problem with running wallet_bridge_api.get_block with incorrect argument type (putting bool as 
   argument). I think, in this situation program should throw an exception. (# BUG2)  

3. Problem with running wallet_bridge_api.get_ops_in_block with incorrect argument (putting negative value as block 
   number). I think, in this situation program should throw an exception.  (# BUG3) 

4. Problem with running wallet_bridge_api.get_ops_in_block with incorrect argument type (putting bool as block 
   number). I think, in this situation program should throw an exception.  (# BUG4) 

5. Problem with running wallet_bridge_api.get_transaction with additional argument.  Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): Transaction id is required as second argument"   (# BUG5) 
   
6. Problem with running wallet_bridge_api.get_order_book with incorvrect argument type (putting bool as argument).
   I think, in this situation program should throw an exception. (# BUG6) 
   
7. Problem with running wallet_bridge_api.get_conversion_requests with additional argument.  Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): account name required as first argument"   (# BUG7) 
   
8. Problem with running wallet_bridge_api.get_collateralized_conversion_requests with additional argument.
   Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): account name required as first argument"   (# BUG8) 
   
9. Problem with running wallet_bridge_api.find_recurrent_transfers with additional argument.  Method return exception:
   "Assert Exception:args.get_array()[0].is_string(): Account name is required as first argument"   (# BUG9) 
   
10. Problem with running wallet_bridge_api.broadcast_transaction with additional argument.  Method return exception:
    "Assert Exception:args.get_array()[0].is_object(): Signed transaction is required as first argument"   (# BUG10) 
"""


def test_get_version(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_version()


def test_get_current_median_history_price(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_current_median_history_price()


def test_get_hardfork_version(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_hardfork_version()


def test_get_chain_properties(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_chain_properties()


def test_get_feed_history(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_feed_history()


def test_get_dynamic_global_properties(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_dynamic_global_properties()


@pytest.mark.parametrize(
    'block_number', [
        0,
        1,
        10,
        18446744073709551615,
    ]
)
def test_get_block_with_correct_value(node, block_number):
    # TODO Add pattern test
    if block_number < 2:  # This condition makes it possible to get a saturated output stream for blocks 0 and 1.
        node.wait_for_block_with_number(2)
    response = node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    'block_number', [
        # -1,  # BUG1
        18446744073709551616,
    ]
)
def test_get_block_with_incorrect_value(node, block_number):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    'block_number', [
        [0],
        # True,  # BUG2
        'incorrect_string_argument'
    ]
)
def test_get_block_with_incorrect_type_of_argument(node, block_number):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        #  BLOCK NUMBER
        (0, True),
        (1, True),  # This test case is important because response have a lot of operations.
        (18446744073709551615, True),

        #  VIRTUAL OPERATION
        (0, True),
        (0, False),
    ]
)
def test_get_ops_in_block_with_correct_value(node, block_number, virtual_operation):
    # TODO Add pattern test
    node.wait_number_of_blocks(21)  # Waiting for next witness schedule
    response = node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        #  BLOCK NUMBER
        # (-1, True),  # BUG3
        (18446744073709551616, True),
    ]
)
def test_get_ops_in_block_with_incorrect_value(node, block_number, virtual_operation):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        #  BLOCK NUMBER
        ('incorrect_string_argument', True),
        # (True, True),  # BUG4
        ([0], True),

        #  VIRTUAL OPERATION
        (0, 'incorrect_string_argument'),
        (0, 0),
        (0, [True]),
    ]
)
def test_get_ops_in_block_with_incorrect_type_of_arguments(node, block_number, virtual_operation):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  ACCOUNT NAME
        ('alice', 'all'),
        ('bob', 'all'),
        ('non-exist-acc', 'all'),  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        ('', 'all'),   # To miejsce mi się nie podoba, tutaj konto jest kluczowe

        #  WITHDRAW ROUTE TYPE
        ('alice', 'all'),
        ('alice', 'incoming'),
        ('alice', 'outgoing'),
    ]
)
def test_get_withdraw_routes_with_correct_value(node, wallet, account_name, withdraw_route_type):
    # TODO Add pattern test
    prepare_node_with_set_convert_request(wallet)
    response = node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  WITHDRAW ROUTE TYPE
        ('alice', 'non-exist-argument'),
    ]
)
def test_get_withdraw_routes_with_incorrect_value(node, wallet, account_name, withdraw_route_type):
    prepare_node_with_set_convert_request(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  ACCOUNT NAME
        (['alice'], 'all'),
        (100, 'all'),
        (True, 'all'),

        #  WITHDRAW ROUTE TYPE
        ('alice', ['all']),
        ('alice', 100),
        ('alice', True),
    ]
)
def test_get_withdraw_routes_with_incorrect_type_of_arguments(node, wallet, account_name, withdraw_route_type):
    prepare_node_with_set_convert_request(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


def test_get_withdraw_routes_with_additional_argument(node, wallet):
    prepare_node_with_set_convert_request(wallet)

    node.api.wallet_bridge.get_withdraw_routes('alice', 'all', 'additional_argument')


def test_get_withdraw_routes_with_missing_argument(node, wallet):
    prepare_node_with_set_convert_request(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_withdraw_routes('alice')


def test_get_transaction_with_correct_value(node, wallet):
    # TODO Add pattern test
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_number_of_blocks(21)
    response = node.api.wallet_bridge.get_transaction(transaction_id)


def test_get_transaction_with_incorrect_value(node):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction('0000000000000000000000000000000000000000')  # non exist transaction id


@pytest.mark.parametrize(
    'transaction_id', [
        'incorrect_string_argument',
        100,
        True,
    ]
)
def test_get_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


def test_get_transaction_with_additional_argument(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_number_of_blocks(21)

    node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')  # BUG5


def test_get_conversion_requests_with_correct_value(node, wallet):
    # TODO Add pattern test
    prepare_node_with_set_convert_request(wallet)

    response = node.api.wallet_bridge.get_conversion_requests('alice')


@pytest.mark.parametrize(
    'account_name', [
        'non-exist-acc',
        '',
    ]
)
def test_get_conversion_requests_with_incorrect_value(node, account_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_conversion_requests(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice'],
        100,
        True,
    ]
)
def test_get_conversion_requests_with_incorrect_type_of_argument(node, wallet, account_name):
    prepare_node_with_set_convert_request(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_conversion_requests(account_name)


def test_get_conversion_requests_with_additional_argument(node, wallet):
    prepare_node_with_set_convert_request(wallet)

    node.api.wallet_bridge.get_conversion_requests('alice', 'additional_argument')  # BUG7


def test_get_collateralized_conversion_requests_with_correct_value(node, wallet):
    # TODO Add pattern test
    prepare_node_with_set_collateral_convert_request(wallet)

    response = node.api.wallet_bridge.get_collateralized_conversion_requests('alice')


@pytest.mark.parametrize(
    'account_name', [
        'non-exist-acc',
        '',
    ]
)
def test_get_collateralized_conversion_requests_with_incorrect_value(node, account_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_conversion_requests(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice'],
        100,
        True,
    ]
)
def test_get_collateralized_conversion_requests_with_incorrect_type_of_argument(node, wallet, account_name):
    prepare_node_with_set_collateral_convert_request(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_collateralized_conversion_requests(account_name)


def test_get_collateralized_conversion_requests_with_additional_argument(node, wallet):
    prepare_node_with_set_collateral_convert_request(wallet)

    node.api.wallet_bridge.get_collateralized_conversion_requests('alice', 'additional_argument')   # BUG8


@pytest.mark.parametrize(
    'orders_limit', [
        0,
        10,
        500,
    ]
)
def test_get_order_book_with_correct_value(node, wallet, orders_limit):
    # TODO Add pattern test
    prepare_node_with_created_order(wallet)

    response = node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        -1,
        501,
    ]
)
def test_get_order_book_with_incorrect_value(node, orders_limit):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        [0],
        # True,  # BUG6
        'incorrect_string_argument'
    ]
)
def test_get_order_book_with_incorrect_type_of_argument(node, orders_limit):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'account_name', [
        '',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        'non-exist-acc',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        'alice',
    ]
)
def test_get_open_orders_with_correct_value(node, wallet, account_name):
    # TODO Add pattern test
    prepare_node_with_created_order(wallet)

    response = node.api.wallet_bridge.get_open_orders(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice'],
        100,
        True
    ]
)
def test_get_open_orders_with_incorrect_type_of_argument(node, wallet, account_name):
    prepare_node_with_created_order(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_open_orders(account_name)


@pytest.mark.parametrize(
    'account_name', [
        '',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        'non-exist-acc',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        'alice',
    ]
)
def test_get_owner_history_with_correct_value(node, wallet, account_name):
    # TODO Add pattern test
    prepare_node_with_updated_account(wallet)

    response = node.api.wallet_bridge.get_owner_history(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice'],
        100,
        True
    ]
)
def test_get_owner_history_with_incorrect_type_of_argument(node, wallet, account_name):
    prepare_node_with_updated_account(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_owner_history(account_name)


def test_is_known_transaction_with_correct_value_and_existing_transaction(node, wallet):
    # TODO Add pattern test
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']

    response = node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        '0000000000000000000000000000000000000000',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        '',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
        '0',  # To miejsce mi się nie podoba, tutaj konto jest kluczowe
    ]
)
def test_is_known_transaction_with_correct_value_and_non_existing_transaction(node, transaction_id):
    node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        ['0000000000000000000000000000000000000000'],
        100,
        True,
    ]
)
def test_is_known_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.is_known_transaction(transaction_id)


def test_get_reward_fund_with_correct_value(node):
    # TODO Add pattern test
    response = node.api.wallet_bridge.get_reward_fund('post')


@pytest.mark.parametrize(
    'reward_fund_name', [
        'command',
        'post0',
        'post1',
        'post2',
        '',
    ]
)
def test_get_reward_fund_with_incorrect_value(node, reward_fund_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        ['post'],
        100,
        True,
    ]
)
def test_get_reward_fund_with_incorrect_type_of_argument(node, reward_fund_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        'alice',
        'bob',
    ]
)
def test_find_recurrent_transfers_with_correct_value(node, wallet, reward_fund_name):
    # TODO Add pattern test
    prepare_node_with_recurrent_transfer(wallet)

    response = node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        'non_exist_account',
        '',
    ]
)
def test_find_recurrent_transfers_with_incorrect_value(node, reward_fund_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        ['alice'],
        100,
        True
    ]
)
def test_find_recurrent_transfers_with_incorrect_type_of_argument(node, wallet, reward_fund_name):
    prepare_node_with_recurrent_transfer(wallet)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


def test_find_recurrent_transfers_with_additional_argument(node, wallet):
    prepare_node_with_recurrent_transfer(wallet)

    node.api.wallet_bridge.find_recurrent_transfers('alice', 'additional_argument')   # BUG9


def test_broadcast_transaction_with_correct_value(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    node.api.wallet_bridge.broadcast_transaction(transaction)

    assert 'alice' in wallet.list_accounts()


@pytest.mark.parametrize(
    'transaction_name', [
        ['non-exist-transaction'],
        'non-exist-transaction',
        100,
        True
    ]
)
def test_broadcast_transaction_with_incorrect_type_of_argument(node, transaction_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.broadcast_transaction(transaction_name)


def test_broadcast_transaction_with_additional_argument(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)

    node.api.wallet_bridge.broadcast_transaction(transaction, 'additional_argument')   # BUG10


def test_broadcast_transaction_synchronous_with_correct_value(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    node.api.wallet_bridge.broadcast_transaction_synchronous(transaction)

    assert 'alice' in wallet.list_accounts()


@pytest.mark.parametrize(
    'transaction_name', [
        ['non-exist-transaction'],
        'non-exist-transaction',
        100,
        True
    ]
)
def test_broadcast_transaction_synchronous_with_incorrect_type_of_argument(node, transaction_name):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.broadcast_transaction_synchronous(transaction_name)


def test_broadcast_transaction_synchronous_with_additional_argument(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.broadcast_transaction_synchronous(transaction, 'additional_argument')


def prepare_node_with_set_withdraw_vesting_route(wallet):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))
    wallet.api.set_withdraw_vesting_route('alice', 'bob', 30, True)


def prepare_node_with_set_convert_request(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer('initminer', 'alice', Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))
    wallet.api.convert_hive_with_collateral('alice', Asset.Test(10))
    wallet.api.convert_hbd('alice', Asset.Tbd(0.1))


def prepare_node_with_set_collateral_convert_request(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer('initminer', 'alice', Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))
    wallet.api.convert_hive_with_collateral('alice', Asset.Test(10))


def prepare_node_with_created_order(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer('initminer', 'alice', Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))
    wallet.api.create_order('alice', 1000, Asset.Test(1), Asset.Tbd(1), False, 1000)


def prepare_node_with_updated_account(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    wallet.api.update_account('alice', '{}', key, key, key, key)


def prepare_node_with_recurrent_transfer(wallet):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))
    wallet.api.transfer('initminer', 'alice', Asset.Test(500), 'memo')
    wallet.api.recurrent_transfer('alice', 'bob', Asset.Test(20), 'memo', 100, 10)
