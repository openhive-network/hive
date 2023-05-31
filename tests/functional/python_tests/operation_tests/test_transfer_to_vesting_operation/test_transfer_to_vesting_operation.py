from datetime import timedelta

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    get_governance_voting_power,
    get_hive_balance,
    get_hive_power,
    get_max_mana,
    get_vesting_price,
    get_virtual_operation,
    hive_to_vest_conversion_value,
)

from hive_local_tools.constants import (
    DELAYED_VOTING_INTERVAL_SECONDS,
    DELAYED_VOTING_TOTAL_INTERVAL_SECONDS,
)


def test_transfer_vests_to_empty_string(prepare_environment):
    """
    User converts Hive to HP and transfers them to own account (empty {to}).
    """
    node, wallet = prepare_environment

    sender = "alice"
    receiver = ""
    amount = tt.Asset.Test(0.001)

    wallet.create_account(sender, hives=1000)
    sender_initial_balance = get_hive_balance(node, sender)
    initial_current_mana = get_max_mana(node, sender)

    price = get_vesting_price(node)
    transaction = wallet.api.transfer_to_vesting(sender, receiver, amount)

    assert (
        sender_initial_balance == get_hive_balance(node, sender) + amount
    ), f"Sender HIVE balance is not reduced by {amount}."
    assert get_hive_power(node, sender) > tt.Asset.Vest(0), f"Receiver HP balance is not increased by {amount}."
    assert hive_to_vest_conversion_value(amount, price) in tt.Asset.Range(
        get_hive_power(node, sender), tolerance=5
    ), "The conversion is done using the wrong exchange rate."
    assert (
        len(get_virtual_operation(node, "transfer_to_vesting_completed_operation")) == 1
    ), "The virtual operation: transfer_to_vesting_completed is not generated."
    assert get_max_mana(node, sender) > initial_current_mana, "RC max_mana is not increased."
    assert get_governance_voting_power(node, wallet, sender) == 0, "The governance voting power is increased."
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."

    node.restart(time_offset=f"+{DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS}s")
    assert get_governance_voting_power(node, wallet, sender) == 0, "The Governance voting power is increased."

    node.restart(
        time_offset=tt.Time.serialize(
            node.get_head_block_time() + timedelta(hours=2),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert get_governance_voting_power(node, wallet, sender) > 0, "the Governance voting power is not increased."
    assert (
        len(get_virtual_operation(node, "delayed_voting_operation")) > 0
    ), "The virtual operation: `delayed_voting_operation` is not generated."
    assert transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."


def test_transfer_vests_to_own_account(prepare_environment):
    """
    User converts Hive to HP and transfers them to own account ({to} = {from}).
    """
    node, wallet = prepare_environment

    sender = receiver = "alice"
    amount = tt.Asset.Test(0.001)

    wallet.create_account(sender, hives=1000)
    sender_initial_balance = get_hive_balance(node, sender)
    initial_current_mana = get_max_mana(node, sender)

    price = get_vesting_price(node)
    transaction = wallet.api.transfer_to_vesting(sender, receiver, amount)

    assert (
        sender_initial_balance == get_hive_balance(node, sender) + amount
    ), f"Sender HIVE balance is not reduced by {amount}."
    assert get_hive_power(node, sender) > tt.Asset.Vest(0), f"Receiver HP balance is not increased by {amount}."
    assert hive_to_vest_conversion_value(amount, price) in tt.Asset.Range(
        get_hive_power(node, sender), tolerance=5
    ), "The conversion is done using the wrong exchange rate."
    assert (
        len(get_virtual_operation(node, "transfer_to_vesting_completed_operation")) == 1
    ), "The virtual operation: transfer_to_vesting_completed is not generated."
    assert get_max_mana(node, sender) > initial_current_mana, "RC max_mana is not increased."
    assert get_governance_voting_power(node, wallet, sender) == 0, "The governance voting power is increased."
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."

    node.restart(time_offset=f"+{DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS}s")
    assert get_governance_voting_power(node, wallet, sender) == 0, "The Governance voting power is increased."

    node.restart(
        time_offset=tt.Time.serialize(
            node.get_head_block_time() + timedelta(hours=2),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert get_governance_voting_power(node, wallet, sender) > 0, "the Governance voting power is not increased."
    assert (
        len(get_virtual_operation(node, "delayed_voting_operation")) > 0
    ), "The virtual operation: `delayed_voting_operation` is not generated."
    assert transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."
    assert get_max_mana(node, sender) > initial_current_mana, "RC max_mana is not increased."


def test_transfer_vests_to_other_account(prepare_environment):
    """
    User converts Hive to HP and transfers them to someone else account.
    """
    node, wallet = prepare_environment

    sender = "alice"
    receiver = "bob"
    amount = tt.Asset.Test(0.001)

    wallet.create_account(sender, hives=1000)
    wallet.create_account(receiver)

    sender_initial_balance = get_hive_balance(node, sender)
    receiver_initial_max_mana = get_max_mana(node, receiver)

    price = get_vesting_price(node)
    transaction = wallet.api.transfer_to_vesting(sender, receiver, amount)

    assert (
        sender_initial_balance == get_hive_balance(node, sender) + amount
    ), f"Sender HIVE balance is not reduced by {amount}."
    assert get_hive_power(node, receiver) > tt.Asset.Vest(0), f"Receiver HP balance is not increased by {amount}."

    assert hive_to_vest_conversion_value(amount, price) in tt.Asset.Range(
        get_hive_power(node, receiver), tolerance=5
    ), "The conversion is done using the wrong exchange rate."
    assert (
        len(get_virtual_operation(node, "transfer_to_vesting_completed_operation")) == 1
    ), "The virtual operation: transfer_to_vesting_completed is not generated."
    assert get_max_mana(node, sender) > receiver_initial_max_mana, "RC max_mana is not increased."
    assert get_governance_voting_power(node, wallet, receiver) == 0, "The governance voting power is increased."
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."

    node.restart(time_offset=f"+{DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS}s")
    assert get_governance_voting_power(node, wallet, receiver) == 0, "The governance voting power is increased."

    node.restart(
        time_offset=tt.Time.serialize(
            node.get_head_block_time() + timedelta(hours=2),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert get_governance_voting_power(node, wallet, receiver) > 0, "The governance voting power is not increased."
    assert (
        len(get_virtual_operation(node, "delayed_voting_operation")) > 0
    ), "The virtual operation: `delayed_voting_operation` is not generated."
    assert transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."


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
    receiver_initial_max_mana = get_max_mana(node, receiver)

    receiver_initial_hive_power_balance = get_hive_power(node, receiver)
    initial_price = get_vesting_price(node)

    first_transaction = wallet.api.transfer_to_vesting(sender, receiver, first_amount)

    initial_time_first_operation = tt.Time.parse(
        node.api.block.get_block(block_num=first_transaction["block_num"])["block"]["timestamp"]
    )

    assert (
        sender_initial_balance == get_hive_balance(node, sender) + first_amount
    ), f"Sender HIVE balance is not reduced by {first_amount}."
    assert (
        get_hive_power(node, receiver) > receiver_initial_hive_power_balance
    ), f"Receiver HP balance is not increased by {first_amount} after operation."
    assert hive_to_vest_conversion_value(first_amount, initial_price) in tt.Asset.Range(
        get_hive_power(node, receiver), tolerance=5
    ), "The conversion is done using the wrong exchange rate."
    assert (
        len(get_virtual_operation(node, "transfer_to_vesting_completed_operation")) == 1
    ), "The virtual operation: transfer_to_vesting_completed is not generated."
    assert get_governance_voting_power(node, wallet, receiver) == 0, "The Governance voting power is increased."
    receiver_max_mana = get_max_mana(node, receiver)
    assert receiver_max_mana > receiver_initial_max_mana, "RC max_mana is not increased."
    assert first_transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.`
    #  Error message: "The cost of RC was not charged for executing the transaction."

    # Jump to day 5
    node.restart(
        time_offset=tt.Time.serialize(
            initial_time_first_operation + timedelta(seconds=5 * DELAYED_VOTING_INTERVAL_SECONDS),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )

    sender_balance = get_hive_balance(node, sender)
    receiver_hp_balance = get_hive_power(node, receiver)
    price_after_time_change = get_vesting_price(node)

    second_transaction = wallet.api.transfer_to_vesting(sender, receiver, second_amount)
    initial_time_second_operation = tt.Time.parse(
        node.api.block.get_block(block_num=second_transaction["block_num"])["block"]["timestamp"]
    )


    assert (
        sender_balance == get_hive_balance(node, sender) + second_amount
    ), f"Sender HIVE balance is not reduced by {second_amount}."
    assert (
        get_hive_power(node, receiver) > receiver_hp_balance
    ), f"Receiver HP balance is not increased by {second_amount} after operation."
    assert hive_to_vest_conversion_value(
        second_amount, price_after_time_change
    ) + receiver_hp_balance in tt.Asset.Range(
        get_hive_power(node, receiver), tolerance=5
    ), "The conversion is done using the wrong exchange rate."
    assert (
        len(get_virtual_operation(node, "transfer_to_vesting_completed_operation")) > 1
    ), "The virtual operation: transfer_to_vesting_completed is not generated."
    assert get_max_mana(node, receiver) > receiver_max_mana, "RC max_mana is not increased."
    assert second_transaction["rc_cost"] > 0, "RC cost is less than or equal to zero."
    # TODO: after resolve #Issue507 add assert for: `RC current_mana is reduced.` (compare to RC after Power up 1).
    #  Error message: "The cost of RC was not charged for executing the transaction."

    # Jump to day 29 (DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS)
    node.restart(
        time_offset=tt.Time.serialize(
            initial_time_first_operation
            + timedelta(seconds=DELAYED_VOTING_TOTAL_INTERVAL_SECONDS - DELAYED_VOTING_INTERVAL_SECONDS),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert get_governance_voting_power(node, wallet, receiver) == 0, "The Governance voting power is increased."

    # Jump to day 30 (DELAYED_VOTING_TOTAL_INTERVAL_SECONDS)
    node.restart(
        time_offset=tt.Time.serialize(
            initial_time_first_operation + timedelta(seconds=DELAYED_VOTING_TOTAL_INTERVAL_SECONDS),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    governance_voting_power = get_governance_voting_power(node, wallet, receiver)
    assert governance_voting_power > 0, "The governance voting power is not increased."
    assert (
        len(get_virtual_operation(node, "delayed_voting_operation")) > 0
    ), "The virtual operation: `delayed_voting_operation` is not generated."

    # Jump to day 34 (29 days after second power up operation)
    node.restart(
        time_offset=tt.Time.serialize(
            initial_time_second_operation + timedelta(seconds=29 * DELAYED_VOTING_INTERVAL_SECONDS),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert (
        get_governance_voting_power(node, wallet, receiver) == governance_voting_power
    ), "The governance voting power is increased."

    # Jump to day 35 (30 days after second power up operation)
    node.restart(
        time_offset=tt.Time.serialize(
            initial_time_second_operation + timedelta(seconds=30 * DELAYED_VOTING_INTERVAL_SECONDS),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert (
        get_governance_voting_power(node, wallet, receiver) > governance_voting_power
    ), "The governance voting power is not increased."
    assert (
        len(get_virtual_operation(node, "delayed_voting_operation")) > 1
    ), "The virtual operation: `delayed_voting_operation` is not generated."
