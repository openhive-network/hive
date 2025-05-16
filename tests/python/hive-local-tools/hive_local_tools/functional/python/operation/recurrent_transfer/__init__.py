from __future__ import annotations

import copy
from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    Account,
    get_hbd_balance,
    get_hive_balance,
    get_transaction_timestamp,
    get_virtual_operations,
)
from schemas.operations.virtual import FailedRecurrentTransferOperation, FillRecurrentTransferOperation

if TYPE_CHECKING:
    from datetime import datetime


class RecurrentTransferAccount(Account):
    def assert_balance_is_reduced_by_transfer(self, amount: tt.Asset.TestT | tt.Asset.TbdT) -> None:
        if isinstance(amount, tt.Asset.TestT):
            assert self.hive - amount == get_hive_balance(
                self._node, self._name
            ), f"The `{self._name}` account balance has not been reduced."
        elif isinstance(amount, tt.Asset.TbdT):
            assert self.hbd - amount == get_hbd_balance(
                self._node, self._name
            ), f"The `{self._name}` account hbd_balance has not been reduced."
        else:
            raise TypeError("Invalid argument type.")

    def assert_balance_is_increased_by_transfer(self, amount: tt.Asset.TestT | tt.Asset.TbdT) -> None:
        if isinstance(amount, tt.Asset.TestT):
            assert self.hive + amount == get_hive_balance(
                self._node, self._name
            ), f"The `{self._name}` account balance has not been increased."
        elif isinstance(amount, tt.Asset.TbdT):
            assert self.hbd + amount == get_hbd_balance(
                self._node, self._name
            ), f"The `{self._name}` account hbd_balance has not been increased."
        else:
            raise TypeError("Invalid argument type.")

    def assert_hives_and_hbds_are_not_changed(self):
        assert self.hive == get_hive_balance(
            self._node, self._name
        ), f"The {self._name}` account balance has been changed."
        assert self.hbd == get_hbd_balance(
            self._node, self._name
        ), f"The {self._name}` account hbd_balance has been changed."


class RecurrentTransfer:
    def __init__(
        self,
        node: tt.InitNode,
        wallet: tt.Wallet,
        from_: str,
        to: str,
        amount: tt.Asset.TestT | tt.Asset.TbdT,
        recurrence: int,
        executions: int,
        pair_id: int | None = None,
    ):
        self._wallet = wallet
        self._node = node
        self._from_ = from_
        self._to = to
        self._amount = amount
        self._memo: str = "{}"
        self._recurrence = recurrence
        self._executions = executions

        if pair_id is not None:
            self._operation_name = "recurrent_transfer_with_id"
            self._pair_id_argument = {"pair_id": pair_id}
        else:
            self._operation_name = "recurrent_transfer"
            self._pair_id_argument = {}
        self._transaction = getattr(wallet.api, self._operation_name)(
            from_=self._from_,
            to=self._to,
            amount=self._amount,
            memo=self._memo,
            recurrence=self._recurrence,
            executions=self._executions,
            **self._pair_id_argument,
        )

        self._timestamp: datetime = get_transaction_timestamp(node, self._transaction)
        self._current_schedule: list[datetime] = self.__get_transfer_schedule(self._timestamp)
        self._executions_schedules: list = [self._current_schedule]
        self._last_execution_time: datetime = self._timestamp
        self._rc_cost: int = self._transaction["rc_cost"]
        self.__assert_minimal_operation_rc_cost()

    @property
    def amount(self):
        return self._amount

    @property
    def executions(self):
        return self._executions

    @property
    def executions_schedules(self):
        return self._executions_schedules

    @property
    def rc_cost(self):
        return self._rc_cost

    @property
    def timestamp(self):
        return self._timestamp

    def __get_transfer_schedule(self, start_timestamp: datetime):
        return [
            start_timestamp + tt.Time.hours(self._recurrence * execution_number)
            for execution_number in range(self._executions)
        ]

    def move_after_last_transfer(self):
        """
        Move forward by {recurrence} time from the last scheduled recurring transfer.
        """
        self._node.restart(
            time_control=tt.StartTimeControl(start_time=self._current_schedule[-1] + tt.Time.hours(self._recurrence))
        )

    def execute_future_transfer(self, execution_date=None):
        actual_head_block_time = self._node.get_head_block_time()
        if not execution_date:
            for num, t in enumerate(self._current_schedule):
                if t > actual_head_block_time:
                    self._node.restart(time_control=tt.StartTimeControl(start_time=self._current_schedule[num]))
                    time_after_restart = self._node.get_head_block_time()
                    self._last_execution_time = self._current_schedule[num]
                    assert time_after_restart >= self._current_schedule[num]
                    break
        else:
            self._node.restart(time_control=tt.StartTimeControl(start_time=execution_date))

    def execute_last_transfer(self):
        self._node.restart(time_control=tt.StartTimeControl(start_time=self._current_schedule[-1]))
        self._node.wait_number_of_blocks(1)
        assert self._node.get_head_block_time() > self._current_schedule[-1]
        self._last_execution_time = self._current_schedule[-1]

    def get_next_execution_date(self):
        return next(date for date in self._current_schedule if self._node.get_head_block_time() < date)

    def assert_fill_recurrent_transfer_operation_was_generated(self, expected_vop: int):
        err = "virtual operation - `fill_recurrent_transfer_operation` hasn't been generated."
        assert len(get_virtual_operations(self._node, FillRecurrentTransferOperation)) == expected_vop, err

    def assert_failed_recurrent_transfer_operation_was_generated(self, expected_vop: int):
        err = "virtual operation - `failed_recurrent_transfer_operation` hasn't been generated."
        assert len(get_virtual_operations(self._node, FailedRecurrentTransferOperation)) == expected_vop, err

    def __assert_minimal_operation_rc_cost(self):
        assert self._rc_cost > 0, "RC cost is less than or equal to zero."

    def update(
        self,
        amount: tt.Asset.AnyT = None,
        new_executions_number: int | None = None,
        new_recurrence_time: int | None = None,
    ):
        self._transaction = getattr(self._wallet.api, self._operation_name)(
            from_=self._from_,
            to=self._to,
            amount=amount if amount is not None else self._amount,
            memo=self._memo,
            recurrence=new_recurrence_time if new_recurrence_time is not None else self._recurrence,
            executions=new_executions_number if new_executions_number is not None else self._executions,
            **self._pair_id_argument,
        )
        self._timestamp = get_transaction_timestamp(self._node, self._transaction)
        self._rc_cost = self._transaction["rc_cost"]
        self.__assert_minimal_operation_rc_cost()

        if new_recurrence_time and not new_executions_number:
            self._current_schedule = [
                self._timestamp + tt.Time.hours(new_recurrence_time * execution_number)
                for execution_number in range(self._executions + 1)
            ][1:]
            self._recurrence = new_recurrence_time
        elif new_executions_number and not new_recurrence_time:
            self._current_schedule = [
                self.get_next_execution_date() + tt.Time.hours(self._recurrence * execution_number)
                for execution_number in range(new_executions_number)
            ]
            self._executions = new_executions_number
        elif new_executions_number and new_recurrence_time:
            self._current_schedule = [
                self._timestamp + tt.Time.hours(new_recurrence_time * execution_number)
                for execution_number in range(new_executions_number + 1)
            ][1:]
            self._executions = new_executions_number
            self._recurrence = new_recurrence_time
        elif not new_executions_number and not new_recurrence_time and amount:
            amount_clone = copy.deepcopy(amount)
            amount_clone.amount = 0
            if amount == amount_clone:
                self._current_schedule = []
            else:
                self._current_schedule = self.__get_transfer_schedule(self.get_next_execution_date())
            self._amount = amount
        self._executions_schedules.append(self._current_schedule)

    def cancel(self) -> None:
        self.update(amount=tt.Asset.Test(0) if isinstance(self._amount, tt.Asset.TestT) else tt.Asset.Tbd(0))


class RecurrentTransferDefinition:
    def __init__(self, amount: tt.Asset.AnyT, recurrence: int, executions: int, pair_id: int) -> None:
        self._amount = amount
        self._executions = executions
        self._pair_id = pair_id
        self._recurrence = recurrence

    @property
    def amount(self) -> tt.Asset.AnyT:
        return self._amount

    @property
    def executions(self) -> int:
        return self._executions

    @property
    def pair_id(self) -> int:
        return self._pair_id

    @property
    def recurrence(self) -> int:
        return self._recurrence
