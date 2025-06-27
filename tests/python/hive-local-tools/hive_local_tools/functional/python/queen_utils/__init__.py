from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from schemas.fields.basic import AccountName, PublicKey
from schemas.fields.compound import Authority
from schemas.operations import (
    AccountCreateOperation,
    DelegateVestingSharesOperation,
    RecurrentTransferOperation,
    TransferOperation,
    TransferToVestingOperation,
)

if TYPE_CHECKING:
    from schemas.fields.assets import AssetHbd, AssetHive


def __create_account(name: str) -> AccountCreateOperation:
    key = tt.PublicKey("secret")
    return AccountCreateOperation(
        creator=AccountName("initminer"),
        new_account_name=AccountName(name),
        json_metadata="{}",
        fee=tt.Asset.Test(0),
        owner=Authority(weight_threshold=1, account_auths=[], key_auths=[(key, 1)]),
        active=Authority(weight_threshold=1, account_auths=[], key_auths=[(key, 1)]),
        posting=Authority(weight_threshold=1, account_auths=[], key_auths=[(key, 1)]),
        memo_key=PublicKey(key),
    )


def __recurrent_transfer(
    sender: str, receiver: str, amount: AssetHive | AssetHbd, recurrence: int = 24, executions: int = 2
) -> RecurrentTransferOperation:
    return RecurrentTransferOperation(
        from_=sender,
        to=receiver,
        amount=amount,
        memo=f"rec_transfer-{sender}",
        recurrence=recurrence,
        executions=executions,
    )


def __transfer(account: str, transfer_asset: AssetHive | AssetHbd) -> TransferOperation:
    return TransferOperation(
        from_="initminer",
        to=account,
        amount=transfer_asset,
        memo=f"supply_transfer-{account}",
    )


def __transfer_to_vesting(account: str, transfer_asset: AssetHive) -> TransferToVestingOperation:
    return TransferToVestingOperation(
        from_="initminer",
        to=account,
        amount=transfer_asset,
    )


def __vesting_delegate(account: str, vests_amount: tt.Asset.Vest) -> DelegateVestingSharesOperation:
    return DelegateVestingSharesOperation(
        delegator="initminer",
        delegatee=account,
        vesting_shares=vests_amount,
    )
