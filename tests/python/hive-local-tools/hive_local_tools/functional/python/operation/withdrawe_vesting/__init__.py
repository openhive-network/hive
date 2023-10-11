from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.constants import VESTING_WITHDRAW_INTERVALS
from hive_local_tools.functional.python.operation import (
    Account,
    Operation,
    get_transaction_timestamp,
    get_vesting_price,
    get_virtual_operations,
)

if TYPE_CHECKING:
    import datetime


class PowerDown(Operation):
    _update_timestamp: datetime = None

    def __init__(self, node, wallet, account_name, vesting_shares, update: bool = False):
        super().__init__(node, wallet)
        self._name = account_name
        self._total_power_down_vesting_shares = vesting_shares
        self._transaction = self._wallet.api.withdraw_vesting(self._name, self._total_power_down_vesting_shares)
        self._timestamp = get_transaction_timestamp(node, self._transaction)
        self._rc_cost = self._transaction["rc_cost"]
        self._vests_to_power_down = tt.Asset.from_(self._transaction["operations"][0][1]["vesting_shares"])
        self._weekly_vest_reduction = self._vests_to_power_down / VESTING_WITHDRAW_INTERVALS
        self._remaining_executions = 13
        self._update_timestamp = get_transaction_timestamp(self._node, self._transaction) if update else None
        self._tranche_schedule = (
            self.__get_tranche_schedule(self._update_timestamp)
            if update
            else self.__get_tranche_schedule(self._timestamp)
        )
        self._virtual_operation_counter = 0
        self._weekly_hive_income = tt.Asset.from_(
            {
                "amount": self._weekly_vest_reduction.amount / get_vesting_price(self._node),
                "precision": 3,
                "nai": "@@000000021",
            }
        )

    @property
    def rc_cost(self) -> int:
        return self._rc_cost

    @property
    def timestamp(self) -> datetime:
        return self._timestamp

    @property
    def weekly_vest_reduction(self) -> tt.Asset.Vest:
        return self._weekly_vest_reduction

    @property
    def weekly_hive_income(self) -> tt.Asset.Test:
        return self._weekly_hive_income

    @property
    def update_timestamp(self) -> None:
        return self._update_timestamp

    @staticmethod
    def convert_to_vest(node, hive: tt.Asset.Test) -> tt.Asset.Vest:
        return tt.Asset.from_({"amount": hive.amount * get_vesting_price(node), "precision": 6, "nai": "@@000000037"})

    @staticmethod
    def __get_tranche_schedule(operation_timestamp: datetime) -> list[datetime]:
        return [
            operation_timestamp + tt.Time.hours(1 * interval) for interval in range(1, VESTING_WITHDRAW_INTERVALS + 1)
        ]

    def cancel(self) -> None:
        self._wallet.api.withdraw_vesting(self._name, tt.Asset.Vest(0))

    def update(self, vesting_shares: tt.Asset.Vest):
        self.__init__(self._node, self._wallet, self._name, vesting_shares, update=True)

    def execute_next_withdraw(self) -> None:
        actual_head_block_time = self._node.get_head_block_time()
        for num, t in enumerate(self._tranche_schedule):
            if t > actual_head_block_time:
                self._node.restart(
                    time_offset=tt.Time.serialize(
                        self._tranche_schedule[num],
                        format_=tt.Time.TIME_OFFSET_FORMAT,
                    )
                )
                self._remaining_executions -= 1
                self._node.wait_for_irreversible_block()
                break

    def execute_last_withdraw(self) -> None:
        self._node.restart(
            time_offset=tt.Time.serialize(
                self._tranche_schedule[-1],
                format_=tt.Time.TIME_OFFSET_FORMAT,
            )
        )
        self._remaining_executions = 0
        self._node.wait_for_irreversible_block()
        assert self._node.get_head_block_time() >= self._tranche_schedule[-1]

    def assert_virtual_operation_fill_vesting_withdraw_operation_was_generated(self, start_block: int = None) -> None:
        week = 13 - self._remaining_executions
        assert (
            len(get_virtual_operations(self._node, "fill_vesting_withdraw_operation", start_block=start_block)) == week
        ), "The virtual operation: fill_vesting_withdraw_operation is not generated."


class PowerDownAccount(Account):
    def fund_vests(self, tests: tt.Asset.Test) -> None:
        self._wallet.api.transfer_to_vesting("initminer", self._name, tests)
        self.update_account_info()

    def assert_hive_power_is_unchanged(self) -> None:
        assert self.get_hive_power() == self._vest, "Hive Power has been changed."

    def assert_hive_power_balance_is_reduced_by_weekly_amount(self, weekly_vest_reduction) -> None:
        error_message = f"{self._name} HP balance is not reduced by {weekly_vest_reduction}."
        assert self.get_hive_power() in tt.Asset.Range(self.vest - weekly_vest_reduction, tolerance=1), error_message

    def assert_hive_balance_is_unchanged(self) -> None:
        assert self.get_hive_balance() == self.hive, "Hive balance has been changed."

    def assert_hive_balance_is_increased_by_weekly_amount(self, weekly_amount) -> None:
        error_message = f"{self._name} HIVE balance is not increased by weekly income."
        assert self.get_hive_balance() in tt.Asset.Range(self.hive + weekly_amount, tolerance=1), error_message
