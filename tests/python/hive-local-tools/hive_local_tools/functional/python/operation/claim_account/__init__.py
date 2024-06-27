from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.functional.python import basic_authority
from hive_local_tools.functional.python.operation import (
    Operation,
    create_transaction_with_any_operation,
    get_transaction_timestamp,
)
from schemas.operations.create_claimed_account_operation import CreateClaimedAccountOperation

if TYPE_CHECKING:
    from datetime import datetime

    from schemas.fields.basic import AccountName


class ClaimAccountToken(Operation):
    def __init__(self, node: tt.InitNode, wallet: tt.Wallet, creator: str, fee: tt.Asset.TestT) -> None:
        super().__init__(node, wallet)
        self._creator: str = creator
        self._fee: tt.Asset.TestT = fee
        self._transaction: dict = self._wallet.api.claim_account_creation(self._creator, self._fee)
        self._rc_cost: int = self._transaction["rc_cost"]

    @property
    def creator(self) -> str:
        return self._creator

    @property
    def fee(self) -> tt.Asset.TestT:
        return self._fee

    @property
    def timestamp(self) -> datetime:
        return get_transaction_timestamp(self._node, self._transaction)


class CreateClaimedAccount(Operation):
    def __init__(
        self, node: tt.InitNode, wallet: tt.Wallet, creator: AccountName, new_account_name: AccountName
    ) -> None:
        super().__init__(node, wallet)
        self._trx = self.generate_transaction(creator, new_account_name)
        self._rc_cost: int = self._trx["rc_cost"]

    def generate_transaction(self, creator: AccountName, new_account_name: AccountName) -> dict:
        public_key = tt.Account(new_account_name).public_key
        return create_transaction_with_any_operation(
            self._wallet,
            CreateClaimedAccountOperation(
                creator=creator,
                new_account_name=new_account_name,
                owner=basic_authority(public_key),
                active=basic_authority(public_key),
                posting=basic_authority(public_key),
                memo_key=public_key,
                json_metadata="{}",
            ),
            broadcast=True,
        )

    @property
    def trx(self) -> dict:
        return self._trx
