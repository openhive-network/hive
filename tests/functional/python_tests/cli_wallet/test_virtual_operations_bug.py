import time
from datetime import datetime, timezone, timedelta
import re

import pytest

from test_tools import Account, Asset, exceptions, logger, Wallet, World
from test_tools.exceptions import CommunicationError


def test_get_account_history(wallet: Wallet, world: World):
    node = world.create_init_node('InitNode1')
    node.run()
    wallet = Wallet(attach_to=node)
    z = node.get_http_endpoint()
    logger.info(z)

    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(20000000))
    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')
    wallet.api.create_account('initminer', 'alice', '{}')
    node.wait_for_block_with_number(10)
    with pytest.raises(exceptions.CommunicationError):
        wallet.api.vote('alice', 'bob', 'hello-world', 97)
    # wallet.api.vote('initminer', 'bob', 'hello-world', 97)

    node.wait_for_block_with_number(31)
    api1 = node.api.account_history.get_account_history(
        account='initminer',
        start=-1,
        limit=1000
    )

    # sprawdź działania buga z "account_history_api.enum_virtual_ops"

    time.sleep(23213)


def test_enum_virtual_ops(wallet: Wallet, world: World):
    node = world.create_init_node('InitNode1')
    node.run()
    wallet = Wallet(attach_to=node)
    z = node.get_http_endpoint()
    logger.info(z)

    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(20000000))
    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')

    ##########################################################accounts creation
    client_accounts = []
    number_of_accounts_in_one_transaction = 110
    number_of_transactions = 1
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0

    accounts = Account.create_multiple(number_of_accounts, 'receiver')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                account_name = accounts[account_number_absolute].name
                account_key = accounts[account_number_absolute].public_key
                wallet.api.create_account('initminer', account_name, '{}')
                client_accounts.append(account_name)
                account_number_absolute = account_number_absolute + 1
            logger.info('Group of accounts created')
    logger.info('End account creations')
    logger.info('End of splitting')

    node.wait_for_block_with_number(15)

    with pytest.raises(exceptions.CommunicationError):
        for y in range(2):
            with wallet.in_single_transaction():
                for x in range(10):
                    wallet.api.vote(client_accounts[x], 'bob', 'hello-world', 98)



    node.wait_for_block_with_number(15 + 21)
    api1 = node.api.account_history.enum_virtual_ops(
        block_range_begin=1,
        block_range_end=40,
        include_reversible=True,
        group_by_block=False,
        operation_begin=0
    )
    time.sleep(23213)


def test_get_ops_in_block(wallet: Wallet, world: World):
    node = world.create_init_node('InitNode1')
    node.run()
    wallet = Wallet(attach_to=node)
    z = node.get_http_endpoint()
    logger.info(z)

    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(20000000))
    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.create_account('initminer', 'tom', '{}')
    wallet.api.create_account('initminer', 'tod', '{}')
    wallet.api.create_account('initminer', 'dan', '{}')
    wallet.api.create_account('initminer', 'nadia', '{}')
    wallet.api.create_account('initminer', 'nastia', '{}')

    node.wait_for_block_with_number(15)
    with pytest.raises(exceptions.CommunicationError):
        with wallet.in_single_transaction():
            wallet.api.vote('alice', 'bob', 'hello-world', 97)
            wallet.api.vote('tom', 'bob', 'hello-world', 97)
            wallet.api.vote('tod', 'bob', 'hello-world', 97)
            wallet.api.vote('dan', 'bob', 'hello-world', 97)
            wallet.api.vote('nadia', 'bob', 'hello-world', 97)
            wallet.api.vote('nastia', 'bob', 'hello-world', 97)

    # wallet.api.vote('initminer', 'bob', 'hello-world', 97)

    node.wait_for_block_with_number(15 + 21)
    api1 = node.api.account_history.get_ops_in_block(
        block_num=15,
        only_virtual=False,
        include_reversible=True
    )

    api2 = node.api.account_history.get_ops_in_block(
        block_num=16,
        only_virtual=False,
        include_reversible=True
    )

    time.sleep(23213)
