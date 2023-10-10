from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.functional.python.operation import Operation, get_transaction_timestamp

if TYPE_CHECKING:
    from datetime import datetime


class WitnessVote(Operation):
    def __init__(self, node, wallet, voter, witness, approve=True) -> None:
        super().__init__(node, wallet)
        self._voter: str = voter
        self._witness: str = witness
        self._transaction: dict = self._wallet.api.vote_for_witness(self._voter, self._witness, approve)
        self._timestamp: datetime = get_transaction_timestamp(node, self._transaction)
        self._rc_cost: int = self._transaction["rc_cost"]
        self.assert_minimal_operation_rc_cost()

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def voter(self) -> str:
        return self._voter

    @property
    def witness(self) -> str:
        return self._witness

    def delete_vote(self) -> None:
        self.__init__(self._node, self._wallet, self._voter, self._witness, approve=False)

    def get_number_of_votes(self) -> list:
        votes = self._node.api.database.list_witness_votes(
            start=[self._voter, ""], limit=1000, order="by_account_witness"
        )["votes"]
        return len([vote for vote in votes if vote["witness"] == self._witness])
