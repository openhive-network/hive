from datetime import datetime, timedelta

from test_tools import Asset


def create_accounts_with_vests_and_tbd(wallet, accounts):
    wallet.create_accounts(len(accounts))
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account, Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer('initminer', account, Asset.Tbd(10000), 'memo')


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


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')
