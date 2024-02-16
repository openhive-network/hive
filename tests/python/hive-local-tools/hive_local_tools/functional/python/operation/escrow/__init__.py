from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.functional.python.operation import Account, Operation, get_virtual_operations
from schemas.operations.virtual.escrow_approved_operation import EscrowApprovedOperation
from schemas.operations.virtual.escrow_rejected_operation import EscrowRejectedOperation

if TYPE_CHECKING:
    import test_tools as tt
    from schemas.transaction import Transaction


class EscrowTransfer(Operation):
    def __init__(
        self, node: tt.InitNode, wallet: tt.Wallet, from_account: Account, agent: Account, to_account: Account
    ) -> None:
        super().__init__(node, wallet)
        self._from_account: Account = from_account
        self._agent: Account = agent
        self._to_account: Account = to_account
        self._created_escrows: int = 0
        self._escrow_trx: dict | None = None

    @property
    def agent(self) -> Account:
        return self._agent

    @property
    def trx(self) -> dict:
        return self._escrow_trx

    @property
    def from_account(self) -> Account:
        return self._from_account

    @property
    def to_account(self) -> Account:
        return self._to_account

    def approve(self, who: Account, approve: bool, escrow_id: int = 0, broadcast: bool = True) -> Transaction:
        return self._wallet.api.escrow_approve(
            self._from_account.name, self._to_account.name, self._agent.name, who.name, escrow_id, approve, broadcast
        )

    def create(
        self,
        hbd_amount: tt.Asset.TbdT,
        hive_amount: tt.Asset.TestT,
        fee: tt.Asset.TbdT,
        ratification_deadline: str,
        escrow_expiration: str,
        json_meta: str = "{}",
        broadcast: bool = True,
    ) -> None:
        self._created_escrows += 1
        self._escrow_trx = self._wallet.api.escrow_transfer(
            self._from_account.name,
            self._to_account.name,
            self._agent.name,
            self._created_escrows - 1,
            hbd_amount,
            hive_amount,
            fee,
            ratification_deadline,
            escrow_expiration,
            json_meta,
            broadcast,
        )

    def dispute(self, who: Account, escrow_id: int = 0, broadcast: bool = True) -> Transaction:
        return self._wallet.api.escrow_dispute(
            self._from_account.name, self._to_account.name, self._agent.name, who.name, escrow_id, broadcast
        )

    def release(
        self,
        who: Account,
        receiver: Account,
        hbd_amount: tt.Asset.TbdT,
        hive_amount: tt.Asset.TestT,
        escrow_id: int = 0,
        broadcast: bool = True,
    ) -> Transaction:
        return self._wallet.api.escrow_release(
            self._from_account.name,
            self._to_account.name,
            self._agent.name,
            who.name,
            receiver.name,
            escrow_id,
            hbd_amount,
            hive_amount,
            broadcast,
        )

    def assert_if_escrow_was_created(self) -> None:
        assert (
            len(self._node.api.database.list_escrows(start=["", 0], limit=1000, order="by_from_id").escrows)
            == self._created_escrows
        ), "Escrow wasn't created"

    def assert_if_escrow_is_disputed(self) -> None:
        escrows = self._node.api.database.list_escrows(start=["", 0], limit=1000, order="by_from_id").escrows
        for escrow in escrows:
            if (
                escrow.agent == self._agent.name
                and escrow.from_ == self._from_account.name
                and escrow.to == self._to_account.name
            ):
                assert escrow.disputed is True, "Escrow is not disputed but it should be."
                return
        raise ValueError("Escrow wasn't found.")

    def assert_escrow_approved_operation_was_generated(self, expected_amount: int) -> None:
        assert (
            len(get_virtual_operations(self._node, EscrowApprovedOperation)) == expected_amount
        ), "escrow_approved virtual operation wasn't generated"

    def assert_escrow_rejected_operation_was_generated(self, expected_amount: int) -> None:
        assert (
            len(get_virtual_operations(self._node, EscrowRejectedOperation)) == expected_amount
        ), "escrow_rejected virtual operation wasn't generated"
