from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.functional.python.operation import Operation, get_transaction_timestamp

if TYPE_CHECKING:
    import test_tools as tt


class UpdateProposalVotes(Operation):
    def __init__(self, node: tt.InitNode, wallet: tt.Wallet, voter: str, proposal_id: int, approve=True) -> None:
        super().__init__(node, wallet)
        self._voter = voter
        self._proposal_id = proposal_id
        self._transaction = wallet.api.update_proposal_votes(voter=voter, proposals=[proposal_id], approve=approve)
        self._rc_cost = self._transaction.rc_cost

    @property
    def rc_cost(self):
        return self._rc_cost

    @property
    def timestamp(self):
        return get_transaction_timestamp(self._node, self._transaction)

    def delete(self):
        self.__init__(self._node, self._wallet, self._voter, self._proposal_id, approve=False)
