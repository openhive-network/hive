"""Tests - operation in Hive - Power down - https://gitlab.syncad.com/hive/hive/-/issues/474"""
import pytest

import test_tools as tt
from hive_local_tools.constants import VESTING_WITHDRAW_INTERVAL_SECONDS, VESTING_WITHDRAW_INTERVALS
from hive_local_tools.functional.python.operation import jump_to_date
from hive_local_tools.functional.python.operation.withdrawe_vesting import PowerDown


@pytest.mark.testnet
def test_power_down(prepared_node, wallet, alice):
    """
    User creates Power down
    """
    power_down = PowerDown(prepared_node, wallet, alice.name, alice.vest)

    alice.assert_hive_power_is_unchanged()
    alice.rc_manabar.assert_max_rc_mana_state("reduced")
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=power_down.rc_cost + power_down.weekly_vest_reduction.amount + 1
    )
    alice.update_account_info()

    for week_number in range(1, VESTING_WITHDRAW_INTERVALS):  # check weekly vests/ hives changes (from week 1 to 12)
        power_down.execute_next_withdraw()

        alice.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)
        alice.assert_hive_balance_is_increased_by_weekly_amount(weekly_amount=power_down.weekly_hive_income)
        alice.rc_manabar.assert_max_rc_mana_state("reduced")
        power_down.assert_virtual_operation_fill_vesting_withdraw_operation_was_generated()

        alice.update_account_info()

    # Jump to week 13 (VESTING_WITHDRAW_INTERVALS)
    power_down.execute_last_withdraw()

    power_down.assert_virtual_operation_fill_vesting_withdraw_operation_was_generated()
    assert alice.get_hive_power() == tt.Asset.Vest(0), f"Account `{alice.name}` Hive Power should be 0, it is not."
    alice.assert_hive_balance_is_increased_by_weekly_amount(weekly_amount=power_down.weekly_hive_income)
    alice.rc_manabar.assert_max_rc_mana_state("unchanged")


@pytest.mark.testnet
def test_cancel_power_down(prepared_node, wallet, alice):
    """
    User wants to stop Power down a few days after creating Power down.
    """
    power_down = PowerDown(prepared_node, wallet, alice.name, alice.vest)

    alice.assert_hive_power_is_unchanged()
    alice.rc_manabar.assert_max_rc_mana_state("reduced")
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=power_down.rc_cost + power_down.weekly_vest_reduction.amount + 1
    )
    alice.update_account_info()

    for week_number in range(1, 3):  # check weekly vests/ hives changes (from week 1 to 3)
        power_down.execute_next_withdraw()

        alice.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)
        alice.assert_hive_balance_is_increased_by_weekly_amount(weekly_amount=power_down.weekly_hive_income)
        alice.rc_manabar.assert_max_rc_mana_state("reduced")
        power_down.assert_virtual_operation_fill_vesting_withdraw_operation_was_generated()

        alice.update_account_info()

    power_down.cancel()

    power_down.execute_next_withdraw()
    alice.assert_hive_power_is_unchanged()
    alice.assert_hive_balance_is_unchanged()
    err_message = "Max rc mana did not return to the state before the canceled `power down`."
    assert alice.get_rc_max_mana() == alice.rc_manabar.max_rc + power_down.weekly_vest_reduction.amount + 1, err_message


@pytest.mark.testnet
@pytest.mark.parametrize(
    "first_pd_amount, second_pd_amount",
    [
        (tt.Asset.Test(3_000), tt.Asset.Test(10_000)),  # User wants to increase the amount of Power down.
        (tt.Asset.Test(10_000), tt.Asset.Test(3_000)),  # User wants to decrease the amount of Power down.
    ],
)
def test_modify_power_down_amount(prepared_node, wallet, alice, first_pd_amount, second_pd_amount):
    first_pd_vest_amount = PowerDown.convert_to_vest(prepared_node, first_pd_amount)
    power_down = PowerDown(prepared_node, wallet, alice.name, first_pd_vest_amount)

    alice.assert_hive_power_is_unchanged()
    alice.rc_manabar.assert_max_rc_mana_state("reduced")
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=power_down.rc_cost + power_down.weekly_vest_reduction.amount + 1
    )
    alice.update_account_info()

    for week_number in range(1, 3):  # check weekly vests/ hives changes from week 1 to 2 (first power down)
        power_down.execute_next_withdraw()

        alice.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)
        alice.assert_hive_balance_is_increased_by_weekly_amount(weekly_amount=power_down.weekly_hive_income)
        alice.rc_manabar.assert_max_rc_mana_state("reduced")
        power_down.assert_virtual_operation_fill_vesting_withdraw_operation_was_generated()
        alice.update_account_info()

    # Jump to 2.5 hours after first power down operation (in mainnet its 2.5 week).
    jump_to_date(prepared_node, power_down.timestamp + tt.Time.seconds(2.5 * VESTING_WITHDRAW_INTERVAL_SECONDS))
    err = "The headblock timing is not between the 2nd and 3rd week of power down."
    assert power_down._tranche_schedule[1] < prepared_node.get_head_block_time() < power_down._tranche_schedule[2], err
    second_pd_vest_amount = PowerDown.convert_to_vest(prepared_node, second_pd_amount)
    power_down.update(second_pd_vest_amount)

    alice.assert_hive_power_is_unchanged()
    alice.assert_hive_balance_is_unchanged()
    power_down.assert_minimal_operation_rc_cost()
    # The reason for RC equalization is that RC is charged upfront based on a higher PD amount.
    # When the power down value is reduced, an adjustment is made to equalize the RC accordingly.
    if first_pd_amount > second_pd_amount:
        alice.rc_manabar.assert_max_rc_mana_state("increased")
    else:
        alice.rc_manabar.assert_max_rc_mana_state("reduced")

    # Jump to time between 2 and 3 week after first power down operation (in mainnet its 2.5 week).
    jump_to_date(prepared_node, power_down.update_timestamp + tt.Time.seconds(VESTING_WITHDRAW_INTERVAL_SECONDS // 2))

    alice.assert_hive_balance_is_unchanged()
    alice.assert_hive_power_is_unchanged()
    alice.update_account_info()

    headblock = prepared_node.get_last_block_number()
    for week_number in range(1, VESTING_WITHDRAW_INTERVALS):  # Jump from 1 to 12 week after update power down operation
        power_down.execute_next_withdraw()
        tt.logger.info(f"num {week_number}")
        power_down.assert_virtual_operation_fill_vesting_withdraw_operation_was_generated(start_block=headblock)
        alice.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)
        alice.assert_hive_balance_is_increased_by_weekly_amount(weekly_amount=power_down.weekly_hive_income)
        alice.rc_manabar.assert_max_rc_mana_state("reduced")
        alice.update_account_info()

    power_down.execute_last_withdraw()  # Jump to 13 week after second power down operation
    power_down.assert_virtual_operation_fill_vesting_withdraw_operation_was_generated(start_block=headblock)
    alice.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)
    alice.assert_hive_balance_is_increased_by_weekly_amount(weekly_amount=power_down.weekly_hive_income)
    alice.rc_manabar.assert_max_rc_mana_state("unchanged")
