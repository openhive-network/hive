from datetime import timedelta

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


def test_increase_the_value_of_the_power_down(node):
    """
    User wants to increase the amount of Power down.
    """
    wallet = tt.Wallet(attach_to=node)

    first_pd_amount = tt.Asset.Test(1_000)
    first_pd_amount_as_vest = tt.Asset.from_(
        {"amount": first_pd_amount.amount / get_vesting_price(node), "precision": 6, "nai": "@@000000037"})

    first_pd_weekly_vest_reduction = first_pd_amount_as_vest / VESTING_WITHDRAW_INTERVALS
    first_pd_weekly_hive_deposit = tt.Asset.from_(
        {
            "amount": first_pd_weekly_vest_reduction.amount / get_vesting_price(node),
            "precision": 3,
            "nai": "@@000000021"
        }
    )

    second_pd_amount = tt.Asset.Test(3_000)
    second_pd_amount_as_vest = tt.Asset.from_(
        {"amount": second_pd_amount.amount / get_vesting_price(node), "precision": 6, "nai": "@@000000037"})

    wallet.create_account(account, vests=first_pd_amount + second_pd_amount)

    alice_balance = get_hive_balance(node, account)
    alice_hive_power = get_hive_power(node, account)
    alice_rc_max_mana = get_rc_max_mana(node, account)

    first_transaction = wallet.api.withdraw_vesting("alice", first_pd_amount_as_vest)
    first_transaction_timestamp = node.get_head_block_time()

    __assert_hive_balance_unchanged(node, account, alice_balance)
    __assert_rc_max_mana_is_reduced(node, account, previous_rc_max_mana=alice_rc_max_mana)
    alice_rc_max_mana = get_rc_max_mana(node, account)
    __assert_hive_power_balance_unchanged(node, account, alice_hive_power)
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."
    __assert_minimal_operation_rc_cost(first_transaction)

    node.wait_for_irreversible_block()

    for week_number in range(1, 3):  # check weekly assets changes from week 1 to 2 (first power down)
        jump_to_date(
            node, first_transaction_timestamp + timedelta(seconds=week_number * VESTING_WITHDRAW_INTERVAL_SECONDS)
        )

        __assert_hive_power_balance_is_reduced_by_amount(node, account, alice_hive_power, first_pd_weekly_vest_reduction)
        __assert_hive_balance_is_increased_by_amount(node, account, alice_balance, first_pd_weekly_hive_deposit)
        __assert_rc_max_mana_is_reduced(node, account, alice_rc_max_mana)
        __assert_virtual_operation_was_generated(node, "fill_vesting_withdraw_operation", week_number)

        alice_hive_power = get_hive_power(node, account)
        alice_balance = get_hive_balance(node, account)
        alice_rc_max_mana = get_rc_max_mana(node, account)

        node.wait_for_irreversible_block()

    # Jump to 2.5 week after first power down operation
    jump_to_date(node, first_transaction_timestamp + timedelta(seconds=2.5 * VESTING_WITHDRAW_INTERVAL_SECONDS))
    second_transaction = wallet.api.withdraw_vesting(account, second_pd_amount_as_vest)  # Cancel power down operation
    second_transaction_timestamp = node.get_head_block_time()

    __assert_hive_power_balance_unchanged(node, account, alice_hive_power)
    __assert_hive_balance_unchanged(node, account, alice_balance)
    __assert_rc_max_mana_is_reduced(node, account, alice_rc_max_mana)
    __assert_minimal_operation_rc_cost(second_transaction)
    node.wait_for_irreversible_block()

    # Jump to 3 weeks after first power down operation
    jump_to_date(node, first_transaction_timestamp + timedelta(seconds=3 * VESTING_WITHDRAW_INTERVAL_SECONDS))

    __assert_hive_power_balance_unchanged(node, account, alice_hive_power)
    __assert_hive_balance_unchanged(node, account, alice_balance)

    alice_hive_power = get_hive_power(node, account)
    alice_balance = get_hive_balance(node, account)
    alice_rc_max_mana = get_rc_max_mana(node, account)
    number_of_vop = len(get_virtual_operation(node, "fill_vesting_withdraw_operation"))

    second_pd_weekly_vest_reduction = second_pd_amount_as_vest / VESTING_WITHDRAW_INTERVALS
    second_pd_weekly_hive_deposit = tt.Asset.from_(
        {
            "amount": second_pd_weekly_vest_reduction.amount / get_vesting_price(node),
            "precision": 3,
            "nai": "@@000000021"
        }
    )

    node.wait_for_irreversible_block()

    for week_number in range(1, VESTING_WITHDRAW_INTERVALS):  # Jump to 1 to 12 week after second power down operation
        jump_to_date(
            node, second_transaction_timestamp + timedelta(seconds=week_number * VESTING_WITHDRAW_INTERVAL_SECONDS)
        )
        __assert_hive_power_balance_is_reduced_by_amount(node, account, alice_hive_power, second_pd_weekly_vest_reduction)
        __assert_hive_balance_is_increased_by_amount(node, account, alice_balance, second_pd_weekly_hive_deposit)
        __assert_rc_max_mana_is_reduced(node, account, alice_rc_max_mana)
        __assert_virtual_operation_was_generated(node, "fill_vesting_withdraw_operation", number_of_vop + week_number)

        alice_hive_power = get_hive_power(node, account)
        alice_balance = get_hive_balance(node, account)
        alice_rc_max_mana = get_rc_max_mana(node, account)

    number_of_vop = len(get_virtual_operation(node, "fill_vesting_withdraw_operation"))

    # Jump to 13 week after second power down operation
    jump_to_date(node, second_transaction_timestamp + timedelta(seconds=13 * VESTING_WITHDRAW_INTERVAL_SECONDS))

    __assert_virtual_operation_was_generated(node, "fill_vesting_withdraw_operation", number_of_vop + 1)
    __assert_hive_balance_is_increased_by_amount(node, account, alice_balance, second_pd_weekly_hive_deposit)
    __assert_hive_power_balance_is_reduced_by_amount(node, account, alice_hive_power, second_pd_weekly_vest_reduction)
    __assert_rc_max_mana_unchanged(node, account, alice_rc_max_mana)
