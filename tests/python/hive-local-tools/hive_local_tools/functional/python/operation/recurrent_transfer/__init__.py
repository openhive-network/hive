from datetime import datetime
from typing import Union

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    Account,
    get_hbd_balance,
    get_hive_balance,
    get_transaction_timestamp,
    get_virtual_operations,
)


class RecurrentTransferAccount(Account):
    def assert_balance_is_reduced_by_transfer(self, amount):
        if isinstance(amount, tt.Asset.Test):
            assert self._hive - amount == get_hive_balance(
                self._node, self._name
            ), f"The `{self._name}` account balance has not been reduced."
        if isinstance(amount, tt.Asset.Tbd):
            assert self._hbd - amount == get_hbd_balance(
                self._node, self._name
            ), f"The {self._name}` account hbd_balance has not been reduced."

    def assert_balance_is_increased_by_transfer(self, amount):
        if isinstance(amount, tt.Asset.Test):
            assert self._hive + amount == get_hive_balance(
                self._node, self._name
            ), f"The {self._name}` account balance has not been increased."
        if isinstance(amount, tt.Asset.Tbd):
            assert self._hbd + amount == get_hbd_balance(
                self._node, self._name
            ), f"The {self._name}` account hbd_balance has not been increased."

    def assert_hives_and_hbds_are_not_changed(self):
        assert self._hive == get_hive_balance(
            self._node, self._name
        ), f"The {self._name}` account balance has been changed."
        assert self._hbd == get_hbd_balance(
            self._node, self._name
        ), f"The {self._name}` account hbd_balance has been changed."


class RecurrentTransfer:
    def __init__(self, node, wallet, from_, to, amount, recurrence, executions, pair_id=None):
        self._wallet: tt.Wallet = wallet
        self._node: tt.InitNode = node
        self._from_: str = from_
        self._to: str = to
        self._amount: Union[tt.Asset.Test, tt.Asset.Tbd] = amount
        self._memo: str = "{}"
        self._recurrence: int = recurrence
        self._executions: int = executions

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
            **self._pair_id_argument
        )

        self._timestamp: datetime = get_transaction_timestamp(node, self._transaction)
        self._current_schedule: list[datetime] = self.__get_transfer_schedule()
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

    def __get_transfer_schedule(self):
        return [
            self._timestamp + tt.Time.hours(self._recurrence * execution_number)
            for execution_number in range(self._executions)
        ]

    def move_after_last_transfer(self):
        """
        Move forward by {recurrence} time from the last scheduled recurring transfer.
        """
        self._node.restart(
            time_offset=tt.Time.serialize(
                self._current_schedule[-1] + tt.Time.hours(self._recurrence),
                format_=tt.Time.TIME_OFFSET_FORMAT,
            )
        )

    def execute_future_transfer(self, execution_date=None):
        actual_head_block_time = self._node.get_head_block_time()
        if not execution_date:
            for num, t in enumerate(self._current_schedule):
                if t > actual_head_block_time:
                    self._node.restart(
                        time_offset=tt.Time.serialize(
                            self._current_schedule[num],
                            format_=tt.Time.TIME_OFFSET_FORMAT,
                        )
                    )
                    time_after_restart = self._node.get_head_block_time()
                    self._last_execution_time = self._current_schedule[num]
                    assert time_after_restart >= self._current_schedule[num]
                    break
        else:
            self._node.restart(
                time_offset=tt.Time.serialize(
                    execution_date,
                    format_=tt.Time.TIME_OFFSET_FORMAT,
                )
            )

    def execute_last_transfer(self):
        self._node.restart(
            time_offset=tt.Time.serialize(
                self._current_schedule[-1],
                format_=tt.Time.TIME_OFFSET_FORMAT,
            )
        )
        self._node.wait_number_of_blocks(1)
        assert self._node.get_head_block_time() > self._current_schedule[-1]
        self._last_execution_time = self._current_schedule[-1]

    def get_next_execution_date(self):
        return [date for date in self._current_schedule if self._node.get_head_block_time() < date][0]

    def assert_fill_recurrent_transfer_operation_was_generated(self, expected_vop: int):
        err = "virtual operation - `fill_recurrent_transfer_operation` hasn't been generated."
        assert len(get_virtual_operations(self._node, "fill_recurrent_transfer_operation")) == expected_vop, err

    def assert_failed_recurrent_transfer_operation_was_generated(self, expected_vop: int):
        err = "virtual operation - `failed_recurrent_transfer_operation` hasn't been generated."
        assert len(get_virtual_operations(self._node, "failed_recurrent_transfer_operation")) == expected_vop, err

    def __assert_minimal_operation_rc_cost(self):
        assert self._rc_cost > 0, "RC cost is less than or equal to zero."

    def update(self, amount: tt.Asset = None, new_executions_number: int = None, new_recurrence_time: int = None):
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

        if amount:
            self._amount = amount
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
        else:
            self._current_schedule = []
        self._executions_schedules.append(self._current_schedule)

    def cancel(self):
        self.update(amount=tt.Asset.Test(0) if isinstance(self._amount, tt.Asset.Test) else tt.Asset.Tbd(0))
