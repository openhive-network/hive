from __future__ import annotations

import test_tools as tt
from schemas.fields.assets import AssetHiveHF26
from schemas.fields.basic import AccountName, PublicKey
from schemas.fields.compound import Authority
from schemas.operations import (
    AccountCreateOperation,
    RecurrentTransferOperation,
    TransferOperation,
    TransferToVestingOperation,
)


def __create_account(name: str) -> AccountCreateOperation:
    key = tt.PublicKey("secret")
    return AccountCreateOperation(
        creator=AccountName("initminer"),
        new_account_name=AccountName(name),
        json_metadata="{}",
        fee=tt.Asset.Test(0),
        owner=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        active=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        posting=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        memo_key=PublicKey(key),
    )


def __recurrent_transfer(account: str, receiver: str, amount) -> RecurrentTransferOperation:
    return RecurrentTransferOperation(
        from_=account,
        to=receiver,
        amount=amount,
        memo=f"rec_transfer-{account}",
        recurrence=24,
        executions=2,
    )


def __transfer(account: str) -> TransferOperation:
    return TransferOperation(
        from_="initminer",
        to=account,
        amount=AssetHiveHF26(amount=4),
        memo=f"supply_transfer-{account}",
    )


def __transfer_to_vesting(account: str) -> TransferToVestingOperation:
    return TransferToVestingOperation(
        from_="initminer",
        to=account,
        amount=AssetHiveHF26(amount=10),
    )
