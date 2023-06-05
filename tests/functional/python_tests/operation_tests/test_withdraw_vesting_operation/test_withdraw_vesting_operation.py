from datetime import timedelta

import test_tools as tt

from hive_local_tools.constants import VESTING_WITHDRAW_INTERVALS, VESTING_WITHDRAW_INTERVAL_SECONDS

from hive_local_tools.functional.python.operation import (
    get_hive_balance,
    get_hive_power,
    get_rc_max_mana,
    get_vesting_price,
    jump_to_date,
)

from hive_local_tools.functional.python.operation.assertions import (
    __assert_hive_balance_is_increased_by_amount,
    __assert_hive_balance_unchanged,
    __assert_hive_power_balance_is_reduced_by_amount,
    __assert_hive_power_balance_unchanged,
    __assert_minimal_operation_rc_cost,
    __assert_rc_max_mana_is_increased,
    __assert_rc_max_mana_is_reduced,
    __assert_rc_max_mana_unchanged,
    __assert_virtual_operation_was_generated,
)

account = "alice"


def test_power_down(node):
    """
    User creates Power down
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(account, vests=13_000)

    alice_balance = get_hive_balance(node, account)
    alice_hive_power = get_hive_power(node, account)
    alice_rc_max_mana = get_rc_max_mana(node, account)

    weekly_vest_reduction = alice_hive_power / VESTING_WITHDRAW_INTERVALS
    weekly_hive_deposit = tt.Asset.from_(
        {
            "amount": weekly_vest_reduction.amount / get_vesting_price(node),
            "precision": 3,
            "nai": "@@000000021"
        }
    )

    transaction = wallet.api.withdraw_vesting(account, alice_hive_power)
    transaction_timestamp = node.get_head_block_time()

    __assert_hive_balance_unchanged(node, account, alice_balance)
    __assert_rc_max_mana_is_reduced(node, account, previous_rc_max_mana=alice_rc_max_mana)
    alice_rc_max_mana = get_rc_max_mana(node, account)
    __assert_hive_power_balance_unchanged(node, account, alice_hive_power)
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."
    __assert_minimal_operation_rc_cost(transaction)

    node.wait_for_irreversible_block()

    for week_number in range(1, VESTING_WITHDRAW_INTERVALS):  # check weekly assets changes (from week 1 to 12)
        jump_to_date(node, transaction_timestamp + timedelta(seconds=week_number * VESTING_WITHDRAW_INTERVAL_SECONDS))

        __assert_hive_power_balance_is_reduced_by_amount(node, account, alice_hive_power, weekly_vest_reduction)
        __assert_hive_balance_is_increased_by_amount(node, account, alice_balance, weekly_hive_deposit)
        __assert_rc_max_mana_is_reduced(node, account, alice_rc_max_mana)
        __assert_virtual_operation_was_generated(node, "fill_vesting_withdraw_operation", week_number)

        alice_hive_power = get_hive_power(node, account)
        alice_balance = get_hive_balance(node, account)
        alice_rc_max_mana = get_rc_max_mana(node, account)

        node.wait_for_irreversible_block()

    # Jump to week 13 (VESTING_WITHDRAW_INTERVALS), check datas after
    jump_to_date(node, transaction_timestamp + timedelta(
        seconds=VESTING_WITHDRAW_INTERVALS * VESTING_WITHDRAW_INTERVAL_SECONDS))

    __assert_virtual_operation_was_generated(node, "fill_vesting_withdraw_operation", 13)
    __assert_hive_power_balance_unchanged(node, account, tt.Asset.Vest(0))  # all vests have been powered down
    __assert_hive_balance_is_increased_by_amount(node, account, alice_balance, weekly_hive_deposit)
    __assert_rc_max_mana_unchanged(node, account, alice_rc_max_mana)


def test_cancel_power_down(node):
    """
    User wants to stop Power down a few days after creating Power down.
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(account, vests=13_000)

    alice_balance = get_hive_balance(node, account)
    alice_hive_power = get_hive_power(node, account)
    alice_rc_max_mana = get_rc_max_mana(node, account)

    weekly_vest_reduction = alice_hive_power / VESTING_WITHDRAW_INTERVALS
    weekly_hive_deposit = tt.Asset.from_(
        {
            "amount": weekly_vest_reduction.amount / get_vesting_price(node),
            "precision": 3,
            "nai": "@@000000021"
        }
    )

    transaction = wallet.api.withdraw_vesting(account, alice_hive_power)
    transaction_timestamp = node.get_head_block_time()

    __assert_hive_balance_unchanged(node, account, alice_balance)
    __assert_rc_max_mana_is_reduced(node, account, previous_rc_max_mana=alice_rc_max_mana)
    alice_rc_max_mana = get_rc_max_mana(node, account)
    __assert_hive_power_balance_unchanged(node, account, alice_hive_power)
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."
    __assert_minimal_operation_rc_cost(transaction)

    node.wait_for_irreversible_block()

    for week_number in range(1, 3):  # check weekly assets changes (from week 1 to 2)
        jump_to_date(node, transaction_timestamp + timedelta(seconds=week_number * VESTING_WITHDRAW_INTERVAL_SECONDS))

        __assert_hive_power_balance_is_reduced_by_amount(node, account, alice_hive_power, weekly_vest_reduction)
        __assert_hive_balance_is_increased_by_amount(node, account, alice_balance, weekly_hive_deposit)
        __assert_rc_max_mana_is_reduced(node, account, alice_rc_max_mana)
        __assert_virtual_operation_was_generated(node, "fill_vesting_withdraw_operation", week_number)

        alice_hive_power = get_hive_power(node, account)
        alice_balance = get_hive_balance(node, account)
        alice_rc_max_mana = get_rc_max_mana(node, account)

        node.wait_for_irreversible_block()

    # Jump to 2.5 week after operation
    jump_to_date(node, transaction_timestamp + timedelta(seconds=2.5 * VESTING_WITHDRAW_INTERVAL_SECONDS))
    wallet.api.withdraw_vesting(account, tt.Asset.Vest(0))  # Cancel power down operation

    alice_balance = get_hive_balance(node, account)
    alice_hive_power = get_hive_power(node, account)

    node.wait_for_irreversible_block()

    # Jump to week 3  (3 * VESTING_WITHDRAW_INTERVAL_SECONDS)
    jump_to_date(node, transaction_timestamp + timedelta(seconds=3 * VESTING_WITHDRAW_INTERVAL_SECONDS))

    __assert_hive_power_balance_unchanged(node, account, alice_hive_power)
    __assert_hive_balance_unchanged(node, account, alice_balance)
    __assert_rc_max_mana_is_increased(node, account, alice_rc_max_mana)

