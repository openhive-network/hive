from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from schemas.fields.assets import AssetHbdHF26, AssetHiveHF26

import test_tools as tt
from schemas.fields.basic import AccountName, PublicKey
from schemas.fields.compound import Authority
from schemas.operations import (
    AccountCreateOperation,
    AccountCreateOperationLegacy,
    DelegateVestingSharesOperation,
    DelegateVestingSharesOperationLegacy,
    RecurrentTransferOperation,
    TransferOperation,
    TransferOperationLegacy,
    TransferToVestingOperation,
    TransferToVestingOperationLegacy,
)
from schemas.operations import (
    RecurrentTransferOperationLegacy as RecurrentTransferOperationLegacy,
)


def __create_account(name: str, legacy_mode: bool) -> AccountCreateOperation | AccountCreateOperationLegacy:
    key = tt.PublicKey("secret")
    return (AccountCreateOperationLegacy if legacy_mode else AccountCreateOperation)(
        creator=AccountName("initminer"),
        new_account_name=AccountName(name),
        json_metadata="{}",
        fee=tt.Asset.Test(0).as_legacy() if legacy_mode else tt.Asset.Test(0),
        owner=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        active=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        posting=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        memo_key=PublicKey(key),
    )


def __recurrent_transfer(
    sender: str, receiver: str, amount: AssetHiveHF26, recurrence: int = 24, executions: int = 2
) -> RecurrentTransferOperation:
    return RecurrentTransferOperation(
        from_=sender,
        to=receiver,
        amount=amount,
        memo=f"rec_transfer-{sender}",
        recurrence=recurrence,
        executions=executions,
    )


def __transfer(
    account: str, transfer_asset: AssetHiveHF26 | AssetHbdHF26, legacy_mode: bool
) -> TransferOperation | TransferOperationLegacy:
    return (TransferOperationLegacy if legacy_mode else TransferOperation)(
        from_="initminer",
        to=account,
        amount=transfer_asset.as_legacy() if legacy_mode else transfer_asset,
        memo=f"supply_transfer-{account}",
    )


def __transfer_to_vesting(
    account: str, transfer_asset: AssetHiveHF26, legacy_mode: bool
) -> TransferToVestingOperation | TransferToVestingOperationLegacy:
    return (TransferToVestingOperationLegacy if legacy_mode else TransferToVestingOperation)(
        from_="initminer",
        to=account,
        amount=transfer_asset,
    )


def __vesting_delegate(
    account: str, vests_amount: tt.Asset.Vest, legacy_mode: bool
) -> DelegateVestingSharesOperation | DelegateVestingSharesOperationLegacy:
    return (DelegateVestingSharesOperationLegacy if legacy_mode else DelegateVestingSharesOperation)(
        delegator="initminer",
        delegatee=account,
        vesting_shares=vests_amount,
    )
