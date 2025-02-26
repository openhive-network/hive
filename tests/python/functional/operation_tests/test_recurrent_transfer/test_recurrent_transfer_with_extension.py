"""Scenarios description: https://gitlab.syncad.com/hive/hive/-/issues/523"""
from __future__ import annotations

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.constants import MIN_RECURRENT_TRANSFERS_RECURRENCE
from hive_local_tools.functional.python.operation import jump_to_date
from hive_local_tools.functional.python.operation.recurrent_transfer import (
    RecurrentTransfer,
    RecurrentTransferAccount,
    RecurrentTransferDefinition,
)

RECURRENT_TRANSFER_DEFINITIONS = [
    RecurrentTransferDefinition(10, 2 * MIN_RECURRENT_TRANSFERS_RECURRENCE, 2, 1),
    RecurrentTransferDefinition(20, MIN_RECURRENT_TRANSFERS_RECURRENCE, 3, 2),
    RecurrentTransferDefinition(30, 2 * MIN_RECURRENT_TRANSFERS_RECURRENCE, 2, 3),
]


@pytest.mark.testnet()
@pytest.mark.parametrize("asset", [tt.Asset.TestT, tt.Asset.TbdT])
def test_recurrent_transfer_with_extension_cases_1_and_2(
    asset: tt.Asset.AnyT,
    recurrent_transfer_setup: tuple[
        tt.InitNode,
        tt.Wallet,
        RecurrentTransferAccount,
        RecurrentTransferAccount,
        RecurrentTransfer,
        RecurrentTransfer,
        RecurrentTransfer,
    ],
):
    """
    User A creates three recurrent transfers in Hive / HBD to user B.
    """
    node, wallet, sender, receiver, rtd1, rtd2, rtd3 = recurrent_transfer_setup

    # Check status after 2 and 3 * {recurrence}
    for object1, object2, vop_amount in zip((rtd1, rtd2), (rtd2, rtd3), (5, 7)):
        object2.execute_future_transfer()
        sender.assert_balance_is_reduced_by_transfer(object1.amount + object2.amount)
        receiver.assert_balance_is_increased_by_transfer(object1.amount + object2.amount)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        object2.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=vop_amount)
        sender.update_account_info()
        receiver.update_account_info()

    rtd3.execute_future_transfer(
        execution_date=node.get_head_block_time() + tt.Time.hours(2 * MIN_RECURRENT_TRANSFERS_RECURRENCE)
    )

    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    rtd3.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=7)


@pytest.mark.testnet()
@pytest.mark.parametrize("asset", [tt.Asset.Test, tt.Asset.Tbd])
def test_recurrent_transfer_with_extension_cases_3_and_4(
    asset: tt.Asset.AnyT,
    recurrent_transfer_setup: tuple[
        tt.InitNode,
        tt.Wallet,
        RecurrentTransferAccount,
        RecurrentTransferAccount,
        RecurrentTransfer,
        RecurrentTransfer,
        RecurrentTransfer,
    ],
):
    """
    User A creates three recurrent transfers in Hive/ HBD to user B and removes two of them.
    """
    node, wallet, sender, receiver, rtd1, rtd2, rtd3 = recurrent_transfer_setup

    for recurrent_transfer in (rtd1, rtd2):
        recurrent_transfer.cancel()
        sender.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
        )
        sender.update_account_info()

    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)

    rtd1.execute_future_transfer(execution_date=rtd1.executions_schedules[0][1])
    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)
    sender.update_account_info()
    receiver.update_account_info()

    jump_to_date(node, time_control=rtd1.timestamp + tt.Time.hours(3 * MIN_RECURRENT_TRANSFERS_RECURRENCE))

    sender.assert_balance_is_reduced_by_transfer(rtd3.amount)
    receiver.assert_balance_is_increased_by_transfer(rtd3.amount)
    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=4)


@pytest.mark.testnet()
@pytest.mark.parametrize("asset", [tt.Asset.Test, tt.Asset.Tbd])
def test_recurrent_transfer_with_extension_cases_5_and_6(
    asset: tt.Asset.AnyT,
    recurrent_transfer_setup: tuple[
        tt.InitNode,
        tt.Wallet,
        RecurrentTransferAccount,
        RecurrentTransferAccount,
        RecurrentTransfer,
        RecurrentTransfer,
        RecurrentTransfer,
    ],
):
    """
    User A creates three recurrent transfers in Hive/ HBD to user B and updates their amounts.
    """
    node, wallet, sender, receiver, rtd1, rtd2, rtd3 = recurrent_transfer_setup

    sender.top_up(asset(100))

    rtd1.update(amount=rtd1.amount + asset(5))
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd1.rc_cost, operation_timestamp=rtd1.timestamp
    )
    sender.update_account_info()

    rtd2.update(amount=rtd2.amount - asset(5))
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd2.rc_cost, operation_timestamp=rtd2.timestamp
    )
    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)
    sender.update_account_info()

    # Check status after 2, 3 and 4 * {recurrence}
    for object1, object2, vop_amount in zip((rtd1, rtd2, rtd1), (rtd2, rtd3, rtd2), (5, 7, 9)):
        object2.execute_future_transfer()
        sender.assert_balance_is_reduced_by_transfer(object1.amount + object2.amount)
        receiver.assert_balance_is_increased_by_transfer(object1.amount + object2.amount)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        object2.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=vop_amount)
        sender.update_account_info()
        receiver.update_account_info()


@pytest.mark.testnet()
@pytest.mark.parametrize("asset", [tt.Asset.Test, tt.Asset.Tbd])
def test_recurrent_transfer_with_extension_cases_7_and_8(
    asset: tt.Asset.AnyT,
    recurrent_transfer_setup: tuple[
        tt.InitNode,
        tt.Wallet,
        RecurrentTransferAccount,
        RecurrentTransferAccount,
        RecurrentTransfer,
        RecurrentTransfer,
        RecurrentTransfer,
    ],
):
    """
    User updates a number of defined recurrent transfer in Hive/ HBD.
    """
    node, wallet, sender, receiver, rtd1, rtd2, rtd3 = recurrent_transfer_setup

    sender.top_up(asset(100))

    rtd1.update(new_executions_number=3)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd1.rc_cost, operation_timestamp=rtd1.timestamp
    )
    sender.update_account_info()

    rtd2.update(new_executions_number=2)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd2.rc_cost, operation_timestamp=rtd2.timestamp
    )

    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)
    sender.update_account_info()

    # Check status after 2 and 3 * {recurrence}
    for object1, object2, vop_amount in zip((rtd1, rtd2), (rtd2, rtd3), (5, 7)):
        object2.execute_future_transfer()
        sender.assert_balance_is_reduced_by_transfer(object1.amount + object2.amount)
        receiver.assert_balance_is_increased_by_transfer(object1.amount + object2.amount)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        object2.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=vop_amount)
        sender.update_account_info()
        receiver.update_account_info()

    # Check status after 4 and 6 * {recurrence}
    for vop_amount in (8, 9):
        rtd1.execute_future_transfer()
        sender.assert_balance_is_reduced_by_transfer(rtd1.amount)
        receiver.assert_balance_is_increased_by_transfer(rtd1.amount)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=vop_amount)
        sender.update_account_info()
        receiver.update_account_info()


@pytest.mark.testnet()
@pytest.mark.parametrize("asset", [tt.Asset.Test, tt.Asset.Tbd])
def test_recurrent_transfer_with_extension_cases_9_and_10(
    asset: tt.Asset.AnyT,
    recurrent_transfer_setup: tuple[
        tt.InitNode,
        tt.Wallet,
        RecurrentTransferAccount,
        RecurrentTransferAccount,
        RecurrentTransfer,
        RecurrentTransfer,
        RecurrentTransfer,
    ],
):
    """
    User updates a frequency of defined recurrent transfers in Hive/ HBD.
    """
    node, wallet, sender, receiver, rtd1, rtd2, rtd3 = recurrent_transfer_setup

    sender.top_up(asset(100))

    rtd1.update(new_recurrence_time=24)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd1.rc_cost, operation_timestamp=rtd1.timestamp
    )
    sender.update_account_info()

    rtd2.update(new_recurrence_time=48)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd2.rc_cost, operation_timestamp=rtd2.timestamp
    )

    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)
    sender.update_account_info()

    # Check status after 2 * {recurrence}:
    jump_to_date(node, rtd1.executions_schedules[0][0] + tt.Time.hours(2 * MIN_RECURRENT_TRANSFERS_RECURRENCE))
    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)

    # Check status after X + {recurrence} and 3 * {recurrence}:
    for object1, vop_amount in zip((rtd1, rtd3), (4, 5)):
        object1.execute_future_transfer()
        sender.assert_balance_is_reduced_by_transfer(object1.amount)
        receiver.assert_balance_is_increased_by_transfer(object1.amount)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        object1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=vop_amount)
        sender.update_account_info()
        receiver.update_account_info()

    # Check status after X + 2 * {recurrence}:
    rtd2.execute_future_transfer()
    sender.assert_balance_is_reduced_by_transfer(rtd1.amount + rtd2.amount)
    receiver.assert_balance_is_increased_by_transfer(rtd1.amount + rtd2.amount)
    sender.rc_manabar.assert_current_mana_is_unchanged()
    rtd2.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=7)
    sender.update_account_info()
    receiver.update_account_info()

    # Check status after 4 and 6 * {recurrence}
    for vop_amount in (8, 9):
        rtd2.execute_future_transfer()
        sender.assert_balance_is_reduced_by_transfer(rtd2.amount)
        receiver.assert_balance_is_increased_by_transfer(rtd2.amount)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        rtd2.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=vop_amount)
        sender.update_account_info()
        receiver.update_account_info()


@pytest.mark.testnet()
@pytest.mark.parametrize("asset", [tt.Asset.Test, tt.Asset.Tbd])
def test_recurrent_transfer_with_extension_cases_11_and_12(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    asset: tt.Asset.AnyT,
):
    """
    User A creates three recurrent transfers in Hive/ HBD, but there is not enough funds for one to be started
    and for second to be executed second time.
    """
    sender.top_up(asset(1020))

    rtd1_amount = asset(1000)
    rtd1 = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=rtd1_amount,
        recurrence=RECURRENT_TRANSFER_DEFINITIONS[0].recurrence,
        executions=RECURRENT_TRANSFER_DEFINITIONS[0].executions,
        pair_id=1,
    )

    sender.assert_balance_is_reduced_by_transfer(rtd1_amount)
    receiver.assert_balance_is_increased_by_transfer(rtd1_amount)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd1.rc_cost, operation_timestamp=rtd1.timestamp
    )
    rtd1.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    jump_to_date(node, time_control=rtd1.timestamp + tt.Time.hours(RECURRENT_TRANSFER_DEFINITIONS[0].recurrence / 2))

    with pytest.raises(ErrorInResponseError) as exception:
        # create recurrent transfer (rtd2) with insufficient resources
        RecurrentTransfer(
            node,
            wallet,
            from_=sender.name,
            to=receiver.name,
            amount=asset(100),
            recurrence=RECURRENT_TRANSFER_DEFINITIONS[1].recurrence,
            executions=RECURRENT_TRANSFER_DEFINITIONS[1].executions,
            pair_id=2,
        )
    assert "Account does not have enough tokens for the first transfer" in exception.value.error

    rtd3_amount = asset(10)
    rtd3 = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=rtd3_amount,
        recurrence=RECURRENT_TRANSFER_DEFINITIONS[2].recurrence,
        executions=RECURRENT_TRANSFER_DEFINITIONS[2].executions,
        pair_id=2,
    )

    sender.assert_balance_is_reduced_by_transfer(rtd3_amount)
    receiver.assert_balance_is_increased_by_transfer(rtd3_amount)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=rtd3.rc_cost, operation_timestamp=rtd3.timestamp
    )
    rtd3.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    rtd1.execute_future_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    rtd1.assert_failed_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    rtd3.execute_future_transfer()
    sender.assert_balance_is_reduced_by_transfer(rtd3_amount)
    receiver.assert_balance_is_increased_by_transfer(rtd3_amount)
    rtd3.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=3)
