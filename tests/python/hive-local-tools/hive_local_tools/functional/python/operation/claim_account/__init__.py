from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.functional.python.operation import Operation, get_transaction_timestamp

if TYPE_CHECKING:
    from datetime import datetime

    import test_tools as tt


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
