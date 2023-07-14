from datetime import timedelta

import pytest

import test_tools as tt

from hive_local_tools.constants import VESTING_WITHDRAW_INTERVALS, VESTING_WITHDRAW_INTERVAL_SECONDS

from hive_local_tools.functional.python.operation import (
    get_hive_balance,
    get_hive_power,
    get_rc_max_mana,
    get_vesting_price,
    get_virtual_operation,
    jump_to_date,
)

from hive_local_tools.functional.python.operation.assertions import (
    __assert_minimal_operation_rc_cost,

)

account = "alice"

class Account:
    def __init__(self, node, wallet, name):
        wallet.create_account(account)
        self.wallet = wallet
        self.node = node
        self.name = name

        self.hives = get_hive_balance(node, name)
        self.vests = get_hive_power(node, name)
        self.max_rc_max_mana = get_rc_max_mana(node, name)

    def fund_vests(self, tests: tt.Asset.Test):
        self.wallet.api.transfer_to_vesting("initminer", self.name, tt.Asset.Test(tests))

    def refresh_resources(self):
        self.hives = get_hive_balance(self.node, self.name)
        self.vests = get_hive_power(self.node, self.name)
        self.max_rc_max_mana = get_rc_max_mana(self.node, self.name)

    def get_actual_rc_max_mana(self):
        return get_rc_max_mana(self.node, self.name)

    def get_actual_hives(self):
        return get_hive_balance(self.node, self.name)

    def get_actual_hive_power(self):
        return get_hive_power(self.node, self.name)


class PowerDown:
    def __init__(self, node, wallet, account_name, power_down_value):
        self.node = node
        self.transaction = wallet.api.withdraw_vesting(account_name, power_down_value)
        self.power_down_timestamp = node.api.block.get_block(block_num=self.transaction["block_num"])["block"]["timestamp"]

        self.vests_to_power_down = tt.Asset.from_(self.transaction["operations"][0][1]["vesting_shares"])
        self.weekly_vest_reduction = self.vests_to_power_down / VESTING_WITHDRAW_INTERVALS

        self.remaining_executions = 13
        self.tranche_schedule = self.__get_tranche_schedule()
        self.weekly_hive_income = tt.Asset.from_(
            {
                "amount": self.weekly_vest_reduction.amount / get_vesting_price(node),
                "precision": 3,
                "nai": "@@000000021"
            }
        )
        self.virtual_operation_counter = 0

    def __get_tranche_schedule(self):
        start = self.power_down_timestamp
        return [tt.Time.parse(start) + tt.Time.hours(1 * w) for w in range(1, VESTING_WITHDRAW_INTERVALS+1)]

    def go_to_next_withdraw(self):
        actual_head_block_time = self.node.get_head_block_time()
        for num, t in enumerate(self.tranche_schedule):
            if t > actual_head_block_time:
                self.node.restart(time_offset=tt.Time.serialize(self.tranche_schedule[num],
                                  format_=tt.Time.TIME_OFFSET_FORMAT,))
                self.remaining_executions -= 1
                time_after_restart = self.node.get_head_block_time()
                self.node.wait_for_irreversible_block()
                break

    def go_to_last_withdraw(self):
        self.node.restart(time_offset=tt.Time.serialize(self.tranche_schedule[-1],
                                                        format_=tt.Time.TIME_OFFSET_FORMAT, ))
        self.remaining_executions -= 1
        self.node.wait_for_irreversible_block()
        assert self.node.get_head_block_time() >= self.tranche_schedule[-1]


class PowerDownCheck:
    def __init__(self, account: Account, pd: PowerDown):
        self.account = account
        self.power_down = pd
        self.generated_virtual_ops = 0

    def rc_max_mana_is_reduced(self) -> None:
        current_rc_max_mana = self.account.get_actual_rc_max_mana()
        assert current_rc_max_mana < self.account.max_rc_max_mana, "RC max_mana is not reduced."

    def rc_max_mana_unchanged(self) -> None:
        current_rc_max_mana = get_rc_max_mana(self.power_down.node, account)
        assert current_rc_max_mana == self.account.max_rc_max_mana, "RC max mana has not changed."

    def hive_power_balance_unchanged(self) -> None:
        error_message = "Hive Power has not changed."
        assert self.account.get_actual_hive_power() == self.account.vests, error_message

    def hive_power_balance_is_reduced_by_weekly_amount(self):
        error_message = f"{account} HP balance is not reduced by {self.power_down.weekly_vest_reduction}."
        assert self.account.get_actual_hive_power() in tt.Asset.Range(self.account.vests - self.power_down.weekly_vest_reduction, tolerance=2), error_message

    def hive_balance_unchanged(self):
        error_message = "Hive balance has not changed."
        assert self.account.get_actual_hives() == self.account.hives, error_message

    def hive_balance_is_increased_by_weekly_amount(self):
        error_message = f"{account} HIVE balance is not incremented by {self.power_down.weekly_hive_income}."
        assert self.account.get_actual_hives() in tt.Asset.Range(self.account.hives + self.power_down.weekly_hive_income, tolerance=5), error_message

    def virtual_operation_was_generated(self):
        week = 13 - self.power_down.remaining_executions
        err = "The virtual operation: fill_vesting_withdraw_operation is not generated."
        assert (len(get_virtual_operation(self.power_down.node, "fill_vesting_withdraw_operation")) == week), err

    def minimal_operation_rc_cost(self):
        assert self.power_down.transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."


def test_power_down(node, wallet):
    """
    User creates Power down
    """
    alice = Account(node, wallet, "alice")
    alice.fund_vests(13_000)
    alice.refresh_resources()

    power_down = PowerDown(node, wallet, alice.name, alice.vests)

    check = PowerDownCheck(alice, power_down)
    check.hive_balance_unchanged()
    check.rc_max_mana_is_reduced()
    check.hive_power_balance_unchanged()
    check.minimal_operation_rc_cost()
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."
    alice.refresh_resources()
    node.wait_for_irreversible_block()

    for week_number in range(1, VESTING_WITHDRAW_INTERVALS):  # check weekly assets changes (from week 1 to 12)
        power_down.go_to_next_withdraw()
        check.rc_max_mana_is_reduced()
        check.hive_power_balance_is_reduced_by_weekly_amount()
        check.hive_balance_is_increased_by_weekly_amount()
        check.virtual_operation_was_generated()
        node.wait_for_irreversible_block()
        alice.refresh_resources()

    # Jump to week 13 (VESTING_WITHDRAW_INTERVALS), check datas after
    power_down.go_to_last_withdraw()

    check.virtual_operation_was_generated()
    check.rc_max_mana_unchanged()
    assert check.account.get_actual_hive_power() == tt.Asset.Vest(0)
    check.hive_balance_is_increased_by_weekly_amount()
