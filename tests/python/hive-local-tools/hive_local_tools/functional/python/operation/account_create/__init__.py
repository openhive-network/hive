from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.functional.python import get_authority
from hive_local_tools.functional.python.operation import (
    Operation,
    create_transaction_with_any_operation,
)
from schemas.operations.account_create_operation import AccountCreateOperation

if TYPE_CHECKING:
    from schemas.fields.basic import AccountName


class AccountCreate(Operation):
    def __init__(
        self, node: tt.InitNode, wallet: tt.Wallet, fee: tt.Asset.TestT, creator: str, new_account_name: str
    ) -> None:
        super().__init__(node, wallet)
        self._creator: str = creator
        self._fee: tt.Asset.TestT = fee
        self._new_account_name: str = new_account_name
        self._trx: dict = self.generate_transaction(fee, creator, new_account_name)
        self._rc_cost: int = self._trx["rc_cost"]

    def generate_transaction(self, fee: tt.Asset.HiveT, creator: AccountName, new_account_name: AccountName) -> dict:
        public_key = tt.Account(new_account_name).public_key
        return create_transaction_with_any_operation(
            self._wallet,
            AccountCreateOperation(
                fee=fee,
                creator=creator,
                new_account_name=new_account_name,
                owner=get_authority(public_key),
                active=get_authority(public_key),
                posting=get_authority(public_key),
                memo_key=public_key,
                json_metadata="{}",
            ),
        )

    @property
    def trx(self) -> dict:
        return self._trx
