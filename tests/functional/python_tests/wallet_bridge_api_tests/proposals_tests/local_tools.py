from collections.abc import Iterable
import json


from datetime import datetime, timedelta

from test_tools import Asset

def convert_bool_or_numeric_to_string(argument):
    if type(argument) in (int, bool):
        return json.dumps(argument)


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


def create_accounts_with_vests_and_tbd(wallet, accounts):
    accounts_in_node = wallet.create_accounts(len(accounts))
    assert get_accounts_name(accounts_in_node) == accounts,\
        " Account names passed to the function are different from the accounts created inside the function"

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account, Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer('initminer', account, Asset.Tbd(10000), 'memo')


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')


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
