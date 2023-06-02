import test_tools as tt
from hive_local_tools.functional.python.operation import (
    convert_hive_to_vest_range,
    get_governance_voting_power,
    get_hive_balance,
    get_hive_power,
    get_rc_max_mana,
    get_virtual_operation,
)


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


def __assert_hive_balance_is_increased_by_amount(node: tt.InitNode, account: str, previous_balance, amount) -> None:
    error_message = f"{account} HIVE balance is not incremented by {amount}."
    assert get_hive_balance(node, account) in tt.Asset.Range(previous_balance + amount, tolerance=2), error_message


def __assert_hive_power_balance_is_increased(node, account: str, amount: tt.Asset.Vest) -> None:
    assert get_hive_power(node, account) > amount, f"{account} HP balance is not increased by {amount}."


def __assert_hive_power_balance_is_reduced_by_amount(node, account: str, previous_balance, amount: tt.Asset.Vest) -> None:
    error_message = f"{account} HP balance is not reduced by {amount}."
    assert get_hive_power(node, account) in tt.Asset.Range(previous_balance-amount, tolerance=2), error_message


def __assert_minimal_operation_rc_cost(transaction) -> None:
    assert transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."


def __assert_power_up_exchange_rate(node: tt.InitNode, account: str, amount: tt.Asset.Test, price: int,
                                    tolerance: int) -> None:
    err = "The conversion is done using the wrong exchange rate."
    assert get_hive_power(node, account) in convert_hive_to_vest_range(amount, price, tolerance=tolerance), err


def __assert_rc_max_mana_is_increased(node: tt.InitNode, account: str, previous_rc_max_mana: int = 0) -> None:
    current_rc_max_mana = get_rc_max_mana(node, account)
    assert current_rc_max_mana > previous_rc_max_mana, "RC max_mana is not increased."


def __assert_rc_max_mana_is_reduced(node: tt.InitNode, account: str, previous_rc_max_mana: int = 0) -> None:
    current_rc_max_mana = get_rc_max_mana(node, account)
    assert current_rc_max_mana < previous_rc_max_mana, "RC max_mana is not reduced."


def __assert_rc_max_mana_unchanged(node: tt.InitNode, account: str, previous_rc_max_mana: int = 0) -> None:
    current_rc_max_mana = get_rc_max_mana(node, account)
    assert current_rc_max_mana == previous_rc_max_mana, "RC max mana has not changed."


def __assert_virtual_operation_was_generated(node: tt.InitNode,
                                             virtual_operation_name: str,
                                             expected_number_of_virtual_operations: int) -> None:
    err = f"The virtual operation: {virtual_operation_name} is not generated."
    assert (len(get_virtual_operation(node, f"{virtual_operation_name}")) == expected_number_of_virtual_operations), err


def __assert_hive_power_balance_unchanged(node, account_name: str, vests: tt.Asset.Vest) -> None:
    error_message = "Hive Power has not changed."
    assert get_hive_power(node, account_name) == vests, error_message


def __assert_hive_balance_unchanged(node, account_name: str, hives: tt.Asset.Test) -> None:
    error_message = "Hive balance has not changed."
    assert get_hive_balance(node, account_name) == hives, error_message
