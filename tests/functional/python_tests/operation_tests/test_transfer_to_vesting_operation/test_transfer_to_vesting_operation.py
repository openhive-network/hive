from datetime import timedelta

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    get_governance_voting_power,
    get_hive_balance,
    get_hive_power,
    get_rc_max_mana,
    get_transaction_timestamp,
    get_vesting_price,
    get_virtual_operation,
    convert_hive_to_vest_range,
    jump_to_date,
)

from hive_local_tools.constants import (
    DELAYED_VOTING_INTERVAL_SECONDS,
    DELAYED_VOTING_TOTAL_INTERVAL_SECONDS,
)


@pytest.mark.parametrize(
    "sender, receiver",
    [
        ("alice", ""),  # User converts Hive to HP and transfers them to own account (empty {to}).
        ("alice", "alice"),  # User converts Hive to HP and transfers them to own account ({to} = {from}).
        ("alice", "bob"),  # User converts Hive to HP and transfers them to someone else account.
    ]
)
def test_transfer_vests(prepare_environment, sender, receiver):
    node, wallet = prepare_environment

    price = get_vesting_price(node)
    amount = tt.Asset.Test(0.001)
    wallet.create_account(sender, hives=1000)
    sender_initial_balance = get_hive_balance(node, sender)
    if receiver and not len(node.api.database.find_accounts(accounts=[receiver])["accounts"]):
        wallet.create_account(receiver)

    transaction = wallet.api.transfer_to_vesting(sender, receiver, amount)

    if not receiver:
        receiver = sender
    operation_timestamp = get_transaction_timestamp(node, transaction)

    __assert_hive_balance_is_reduced_by_amount(node, sender, sender_initial_balance, amount)
    __assert_hive_power_balance_is_increased(node, receiver, tt.Asset.Vest(0))
    __assert_power_up_exchange_rate(node, receiver, amount, price, tolerance=5)
    __assert_virtual_operation_was_generated(node, "transfer_to_vesting_completed_operation",
                                             expected_number_of_virtual_operations=1)
    __assert_governance_voting_power_is_not_increased(node, wallet, receiver, initial_governance_voting_power=0)
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."
    __assert_rc_max_mana_is_increased(node, receiver, initial_rc_max_mana=0)
    __assert_minimal_operation_rc_cost(transaction)

    # Jump to day 29
    jump_to_date(node, operation_timestamp + timedelta(
        seconds=DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS))

    __assert_governance_voting_power_is_not_increased(node, wallet, receiver, initial_governance_voting_power=0)

    # Jump to day 30
    jump_to_date(node, operation_timestamp + timedelta(seconds=DELAYED_VOTING_TOTAL_INTERVAL_SECONDS))

    __assert_governance_voting_power_is_increased(node, wallet, receiver, initial_governance_voting_power=0)
    # The first virtual operation `delayed_voting_operation` is generated during the preparation of the test environment
    __assert_virtual_operation_was_generated(node, "delayed_voting_operation", expected_number_of_virtual_operations=2)


def test_transfer_vests_twice(prepare_environment):
    """
    User converts Hive to HP (Power Up 1) and after 5 days user converts Hive to HP again (Power Up 2).
    """
    node, wallet = prepare_environment

    sender = "alice"
    receiver = "bob"
    first_amount = tt.Asset.Test(1)
    second_amount = tt.Asset.Test(2)

    wallet.create_account(sender, hives=1000)
    wallet.create_account(receiver)

    sender_initial_balance = get_hive_balance(node, sender)
    receiver_initial_max_mana = get_rc_max_mana(node, receiver)

    receiver_initial_hive_power_balance = get_hive_power(node, receiver)
    initial_price = get_vesting_price(node)

    first_transaction = wallet.api.transfer_to_vesting(sender, receiver, first_amount)
    initial_time_first_operation = get_transaction_timestamp(node, first_transaction)

    __assert_hive_balance_is_reduced_by_amount(node, sender, sender_initial_balance, first_amount)
    __assert_hive_power_balance_is_increased(node, receiver, receiver_initial_hive_power_balance)
    __assert_power_up_exchange_rate(node, receiver, first_amount, initial_price, tolerance=5)
    __assert_virtual_operation_was_generated(node, "transfer_to_vesting_completed_operation",
                                             expected_number_of_virtual_operations=1)
    __assert_governance_voting_power_is_not_increased(node, wallet, receiver, 0)
    __assert_rc_max_mana_is_increased(node, receiver, initial_rc_max_mana=receiver_initial_max_mana)
    __assert_minimal_operation_rc_cost(first_transaction)
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."

    # Jump to day 5
    jump_to_date(node, initial_time_first_operation + timedelta(seconds=5 * DELAYED_VOTING_INTERVAL_SECONDS))

    sender_balance = get_hive_balance(node, sender)
    receiver_hp_balance = get_hive_power(node, receiver)
    receiver_rc_max_mana = get_rc_max_mana(node, receiver)
    price_after_time_change = get_vesting_price(node)

    second_transaction = wallet.api.transfer_to_vesting(sender, receiver, second_amount)
    initial_time_second_operation = get_transaction_timestamp(node, second_transaction)

    __assert_hive_balance_is_reduced_by_amount(node, sender, sender_balance, second_amount)
    __assert_hive_power_balance_is_increased(node, receiver, receiver_hp_balance)
    __assert_power_up_exchange_rate(node, receiver, first_amount + second_amount, price_after_time_change, tolerance=5)
    __assert_virtual_operation_was_generated(node, "transfer_to_vesting_completed_operation",
                                             expected_number_of_virtual_operations=2)
    __assert_rc_max_mana_is_increased(node, receiver, initial_rc_max_mana=receiver_rc_max_mana)
    __assert_minimal_operation_rc_cost(second_transaction)
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.` (compare to RC after Power up 1).
    #  Error message: "The cost of RC was not charged for executing the transaction."

    # Jump to day 29 (DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS)
    jump_to_date(node, initial_time_first_operation + timedelta(
        seconds=DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS))

    __assert_governance_voting_power_is_not_increased(node, wallet, receiver, initial_governance_voting_power=0)

    # Jump to day 30 (DELAYED_VOTING_TOTAL_INTERVAL_SECONDS)
    jump_to_date(node, initial_time_first_operation + timedelta(seconds=DELAYED_VOTING_TOTAL_INTERVAL_SECONDS))

    governance_voting_power = get_governance_voting_power(node, wallet, receiver)
    __assert_governance_voting_power_is_increased(node, wallet, receiver, initial_governance_voting_power=0)
    # The first virtual operation `delayed_voting_operation` is generated during the preparation of the test environment
    __assert_virtual_operation_was_generated(node, "delayed_voting_operation", expected_number_of_virtual_operations=2)

    # Jump to day 34 (29 days after second power up operation)
    jump_to_date(node, initial_time_second_operation + timedelta(seconds=29 * DELAYED_VOTING_INTERVAL_SECONDS))

    __assert_governance_voting_power_is_not_increased(node, wallet, receiver,
                                                      initial_governance_voting_power=governance_voting_power)

    # Jump to day 35 (30 days after second power up operation)
    jump_to_date(node, initial_time_second_operation + timedelta(seconds=30 * DELAYED_VOTING_INTERVAL_SECONDS))

    __assert_governance_voting_power_is_increased(node, wallet, receiver,
                                                  initial_governance_voting_power=governance_voting_power)
    # The first virtual operation `delayed_voting_operation` is generated during the preparation of the test environment
    __assert_virtual_operation_was_generated(node, "delayed_voting_operation", expected_number_of_virtual_operations=3)


def __assert_governance_voting_power_is_increased(node: tt.InitNode, wallet: tt.Wallet, account: str,
                                                  initial_governance_voting_power: int = 0) -> None:
    error_message = "The governance voting power is not increased."
    assert get_governance_voting_power(node, wallet, account) > initial_governance_voting_power, error_message


def __assert_governance_voting_power_is_not_increased(node: tt.InitNode, wallet: tt.Wallet, account: str,
                                                      initial_governance_voting_power: int = 0) -> None:
    error_message = "The governance voting power is increased."
    assert get_governance_voting_power(node, wallet, account) == initial_governance_voting_power, error_message


def __assert_hive_balance_is_reduced_by_amount(node: tt.InitNode, account: str, initial_balance, amount) -> None:
    error_message = f"{account} HIVE balance is not reduced by {amount}."
    assert get_hive_balance(node, account) + amount == initial_balance, error_message


def __assert_hive_power_balance_is_increased(node, account: str, amount: tt.Asset.Vest) -> None:
    assert get_hive_power(node, account) > amount, f"{account} HP balance is not increased by {amount}."


def __assert_minimal_operation_rc_cost(transaction) -> None:
    assert transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."


def __assert_power_up_exchange_rate(node: tt.InitNode, account: str, amount: tt.Asset.Test, price: int,
                                    tolerance: int) -> None:
    err = "The conversion is done using the wrong exchange rate."
    assert get_hive_power(node, account) in convert_hive_to_vest_range(amount, price, tolerance=tolerance), err


def __assert_rc_max_mana_is_increased(node: tt.InitNode, account: str, initial_rc_max_mana: int = 0) -> None:
    assert get_rc_max_mana(node, account) > initial_rc_max_mana, "RC max_mana is not increased."


def __assert_virtual_operation_was_generated(node: tt.InitNode,
                                             virtual_operation_name: str,
                                             expected_number_of_virtual_operations: int) -> None:
    err = f"The virtual operation: {virtual_operation_name} is not generated."
    assert (len(get_virtual_operation(node, f"{virtual_operation_name}")) == expected_number_of_virtual_operations), err
