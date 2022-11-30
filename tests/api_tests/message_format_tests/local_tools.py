import json
from typing import Iterable

import test_tools as tt


def create_and_cancel_vesting_delegation(wallet, delegator, delegatee):
    wallet.api.delegate_vesting_shares(delegator, delegatee, tt.Asset.Vest(5))
    # delegation of 0 removes the delegation
    wallet.api.delegate_vesting_shares(delegator, delegatee, tt.Asset.Vest(0))


def create_proposal(wallet, account_name):
    wallet.api.post_comment(account_name, 'test-permlink', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal(account_name,
                               account_name,
                               tt.Time.from_now(weeks=2),
                               tt.Time.from_now(weeks=50),
                               tt.Asset.Tbd(5),
                               'test subject',
                               'test-permlink')


def request_account_recovery(wallet, account_name):
    recovery_account_key = tt.Account('initminer').public_key
    # 'initminer' account is listed as recovery_account in 'alice' and only he has 'power' to recover account.
    # That's why initminer's key is in new 'alice' authority.
    authority = {"weight_threshold": 1, "account_auths": [], "key_auths": [[recovery_account_key, 1]]}
    wallet.api.request_account_recovery('initminer', account_name, authority)


def prepare_escrow(wallet, *, sender: str) -> None:
    wallet.create_account(sender, hives=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                          hbds=tt.Asset.Tbd(100))

    for name in ['receiver', 'agent']:
        wallet.api.create_account(sender, name, '{}')

    wallet.api.escrow_transfer(sender, 'receiver', 'agent', 1, tt.Asset.Tbd(25), tt.Asset.Test(50), tt.Asset.Tbd(1),
                               tt.Time.from_now(weeks=48),
                               tt.Time.from_now(weeks=50),
                               '{}')


def transfer_and_withdraw_from_savings(wallet, account_name):
    wallet.api.transfer_to_savings(account_name, account_name, tt.Asset.Test(50), 'test transfer to savings')
    wallet.api.transfer_from_savings(account_name, 124, account_name, tt.Asset.Test(5), 'test transfer from savings')


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
