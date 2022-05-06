import json

from collections.abc import Iterable
from datetime import datetime, timedelta

from test_tools import Account, Asset, logger, Wallet


def as_string(value):
    if isinstance(value, str):
        return value

    if isinstance(value, Iterable):
        return [as_string(item) for item in value]

    return json.dumps(value)


def test_as_string():
    assert as_string(10) == '10'
    assert as_string(True) == 'true'
    assert as_string('string') == 'string'
    assert as_string([12, True, 'string']) == ['12', 'true', 'string']
    assert as_string([10, True, 'str', ['str', [False, 12]]]) == ['10', 'true', 'str', ['str', ['false', '12']]]


def create_account_and_create_order(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, Asset.Test(100))
    wallet.api.create_order(account_name, 1000, Asset.Test(1), Asset.Tbd(1), False, 1000)


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')


def create_accounts_with_vests_and_tbd(wallet, accounts):
    created_accounts = wallet.create_accounts(len(accounts))
    assert get_accounts_name(created_accounts) == accounts

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account, Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer('initminer', account, Asset.Tbd(10000), 'memo')


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def prepare_proposals(wallet, accounts):
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.post_comment(account, 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    with wallet.in_single_transaction():
        for account_number in range(len(accounts)):
            wallet.api.create_proposal(accounts[account_number],
                                       accounts[account_number],
                                       date_from_now(weeks=account_number * 10),
                                       date_from_now(weeks=account_number * 10 + 5),
                                       Asset.Tbd(account_number * 100),
                                       f'subject-{account_number}',
                                       'permlink')


def prepare_node_with_witnesses(world, witnesses_names):
    node = world.create_init_node()
    for name in witnesses_names:
        witness = Account(name)
        node.config.witness.append(witness.name)
        node.config.private_key.append(witness.private_key)

    node.run()
    wallet = Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for name in witnesses_names:
            wallet.api.create_account('initminer', name, '')

    with wallet.in_single_transaction():
        for name in witnesses_names:
            wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))

    with wallet.in_single_transaction():
        for name in witnesses_names:
            wallet.api.update_witness(
                name, "https://" + name,
                Account(name).public_key,
                {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    logger.info('Waiting for next witness schedule...')
    node.wait_for_block_with_number(22)

    return node

