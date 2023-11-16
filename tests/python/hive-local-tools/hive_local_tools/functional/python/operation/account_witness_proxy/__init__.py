from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.functional.python.operation import Operation, get_transaction_timestamp

if TYPE_CHECKING:
    import test_tools as tt


class AccountWitnessProxy(Operation):
    def __init__(self, node: tt.InitNode, wallet: tt.Wallet, account_to_modify, proxy) -> None:
        super().__init__(node, wallet)
        self._proxy: str = proxy
        self._account_to_modify: str = account_to_modify
        self._transaction: dict = self._wallet.api.set_voting_proxy(self._account_to_modify, self._proxy)
        self._rc_cost: int = self._transaction["rc_cost"]

    @property
    def timestamp(self):
        return get_transaction_timestamp(self._node, self._transaction)
