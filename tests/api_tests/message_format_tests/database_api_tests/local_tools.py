import json
import os
import subprocess
from typing import Optional

from test_tools import Account, Asset

from ..local_tools import date_from_now


def create_account_and_fund_it(wallet, name: str, tests: Optional[Asset.Test] = None,
                               vests: Optional[Asset.Test] = None, tbds: Optional[Asset.Tbd] = None):
    assert any(asset is not None for asset in [tests, vests, tbds]), 'You forgot to fund account'

    wallet.api.create_account('initminer', name, '{}')

    if tests is not None:
        wallet.api.transfer('initminer', name, tests, 'memo')

    if vests is not None:
        wallet.api.transfer_to_vesting('initminer', name, vests)

    if tbds is not None:
        wallet.api.transfer('initminer', name, tbds, 'memo')


def create_and_cancel_vesting_delegation(wallet, delegator, delegatee):
    wallet.api.delegate_vesting_shares(delegator, delegatee, Asset.Vest(5))
    # delegation of 0 removes the delegation
    wallet.api.delegate_vesting_shares(delegator, delegatee, Asset.Vest(0))


def create_proposal(wallet, account_name):
    wallet.api.post_comment(account_name, 'test-permlink', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal(account_name, account_name, date_from_now(weeks=2), date_from_now(weeks=50), Asset.Tbd(5),
                               'test subject', 'test-permlink')


def generate_sig_digest(transaction: dict, private_key: str):
    # sig_digest is mix of chain_id and transaction data. To get it, it's necessary to run sign_transaction executable file
    output = subprocess.check_output([os.environ['SIGN_TRANSACTION_PATH']],
                                     input=f'{{ "tx": {json.dumps(transaction)}, '
                                           f'"wif": "{private_key}" }}'.encode(encoding='utf-8')).decode('utf-8')
    return json.loads(output)['result']['sig_digest']


def prepare_escrow(wallet, *, sender: str) -> None:
    create_account_and_fund_it(wallet, sender, tests=Asset.Test(100), vests=Asset.Test(100), tbds=Asset.Tbd(100))

    for name in ['receiver', 'agent']:
        wallet.api.create_account(sender, name, '{}')

    wallet.api.escrow_transfer(sender, 'receiver', 'agent', 123, Asset.Tbd(25), Asset.Test(50), Asset.Tbd(1),
                               date_from_now(weeks=48), date_from_now(weeks=50), '{}')


def request_account_recovery(wallet, account_name):
    recovery_account_key = Account('initminer').public_key
    # 'initminer' account is listed as recovery_account in 'alice' and only he has 'power' to recover account.
    # That's why initminer's key is in new 'alice' authority.
    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[recovery_account_key, 1]]}
    wallet.api.request_account_recovery('initminer', account_name, authority)


def transfer_and_withdraw_from_savings(wallet, account_name):
    wallet.api.transfer_to_savings(account_name, account_name, Asset.Test(50), 'test transfer to savings')
    wallet.api.transfer_from_savings(account_name, 124, account_name, Asset.Test(5), 'test transfer from savings')
