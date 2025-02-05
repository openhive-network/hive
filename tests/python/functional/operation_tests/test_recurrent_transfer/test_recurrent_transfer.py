"""
Test scenarios: https://gitlab.syncad.com/hive/hive/-/issues/484
"""
from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.constants import (
    MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES,
    MAX_RECURRENT_TRANSFER_END_DATE,
    MIN_RECURRENT_TRANSFERS_RECURRENCE,
)
from hive_local_tools.functional.python.operation import get_virtual_operations
from hive_local_tools.functional.python.operation.recurrent_transfer import RecurrentTransfer, RecurrentTransferAccount
from schemas.operations.virtual import FailedRecurrentTransferOperation


@pytest.mark.testnet()
@pytest.mark.parametrize(("amount", "executions"), [(tt.Asset.Test(10), 3), (tt.Asset.Tbd(10), 3)])
def test_recurrent_transfer_cases_1_and_2(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount,
    executions,
):
    """
    User creates a recurrent transfer in Hive / HBD to be sent once a day for three days
    """
    for execution in range(executions):
        if execution == 0:
            recurrent_transfer = RecurrentTransfer(
                node,
                wallet,
                from_=sender.name,
                to=receiver.name,
                amount=amount,
                recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
                executions=executions,
            )
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            node.wait_for_irreversible_block()
            recurrent_transfer.execute_future_transfer()
            sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1)

        sender.update_account_info()
        receiver.update_account_info()

    node.wait_for_irreversible_block()
    recurrent_transfer.move_after_last_transfer()
    receiver.assert_hives_and_hbds_are_not_changed()
    sender.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=executions)


@pytest.mark.testnet()
@pytest.mark.parametrize(("amount", "executions"), [(tt.Asset.Test(10), 3), (tt.Asset.Tbd(10), 3)])
def test_recurrent_transfer_cases_3_and_4(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount,
    executions,
):
    """
    User removes a defined recurrent transfer in Hive / HBD.
    """
    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()

    sender.rc_manabar.assert_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.cancel()

    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
    )
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer(recurrent_transfer.executions_schedules[0][-1])
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount_1", "amount_2", "executions_1", "executions_2"),
    [
        (tt.Asset.Test(10), tt.Asset.Test(20), 4, 2),  # User increases an amount of defined recurrent transfer in Hive.
        (tt.Asset.Tbd(10), tt.Asset.Tbd(20), 4, 2),  # User increases an amount of defined recurrent transfer in HBD.
        (tt.Asset.Test(20), tt.Asset.Test(10), 4, 2),  # User decreases an amount of defined recurrent transfer in Hive.
        (tt.Asset.Tbd(20), tt.Asset.Tbd(10), 4, 2),  # User decreases an amount of defined recurrent transfer in HBD.
    ],
)
def test_recurrent_transfer_cases_5_6_7_8(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount_1,
    amount_2,
    executions_1,
    executions_2,
):
    for execution in range(executions_1 // 2):
        if execution == 0:
            recurrent_transfer = RecurrentTransfer(
                node,
                wallet,
                from_=sender.name,
                to=receiver.name,
                amount=amount_1,
                recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
                executions=executions_1,
            )

            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            recurrent_transfer.execute_future_transfer()
            sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount_1)
        receiver.assert_balance_is_increased_by_transfer(amount_1)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1)

        sender.update_account_info()
        receiver.update_account_info()

    for execution in range(executions_2):
        if execution == 0:
            recurrent_transfer.update(amount=amount_2, new_executions_number=executions_2)
            sender.rc_manabar.assert_rc_current_mana_is_reduced(
                operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
            )
            sender.assert_hives_and_hbds_are_not_changed()
            receiver.assert_hives_and_hbds_are_not_changed()
            recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
        else:
            recurrent_transfer.execute_future_transfer()
            sender.rc_manabar.assert_current_mana_is_unchanged()
            sender.assert_balance_is_reduced_by_transfer(amount_2)
            receiver.assert_balance_is_increased_by_transfer(amount_2)
            recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 2)
        sender.update_account_info()
        receiver.update_account_info()

    recurrent_transfer.move_after_last_transfer()
    sender.rc_manabar.assert_current_mana_is_unchanged()
    receiver.rc_manabar.assert_current_mana_is_unchanged()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount", "executions", "update_executions"),
    [
        (tt.Asset.Test(10), 3, 5),
        (tt.Asset.Tbd(10), 3, 5),
    ],
)
def test_recurrent_transfer_cases_9_and_10(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount,
    executions,
    update_executions,
):
    """
    User increases a number of defined recurrent transfer in Hive / HBD.
    """
    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions,
    )

    for execution in range(recurrent_transfer.executions - 1):
        if execution == 0:
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1)
        sender.update_account_info()
        receiver.update_account_info()

        if execution < recurrent_transfer.executions - 2:
            recurrent_transfer.execute_future_transfer()

    # After 2 of 3 executions - user increases the number of execution in an existing recurrent transfer.
    assert node.get_head_block_time() < recurrent_transfer.get_next_execution_date()
    recurrent_transfer.update(new_executions_number=update_executions)
    recurrent_transfer.execute_future_transfer()

    for execution in range(recurrent_transfer.executions):
        if execution == 0:
            sender.rc_manabar.assert_rc_current_mana_is_reduced(
                operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
            )
        else:
            sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1 + 2)

        sender.update_account_info()
        receiver.update_account_info()

        recurrent_transfer.execute_future_transfer()

    recurrent_transfer.move_after_last_transfer()

    if isinstance(amount, tt.Asset.TestT):
        assert sender.hive == tt.Asset.Test(10)
        # 2 executions from first transfer and 5 executions from second transfer
        assert receiver.hive == (2 + 5) * amount
    if isinstance(amount, tt.Asset.TbdT):
        assert sender.hbd == tt.Asset.Tbd(10)
        # 2 executions from first transfer and 5 executions from second transfer
        assert receiver.hbd == (2 + 5) * amount


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount", "executions", "update_executions"),
    [
        (tt.Asset.Test(10), 4, 3),
        (tt.Asset.Tbd(10), 4, 3),
    ],
)
def test_recurrent_transfer_cases_11_and_12(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount,
    executions,
    update_executions,
):
    """
    User decreases a number of defined recurrent transfer in Hive / HBD.
    """
    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    sender.rc_manabar.assert_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    # After 2 of 4 executions - user decrease to 3 the number of execution in an existing recurrent transfer.
    recurrent_transfer.update(new_executions_number=update_executions)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
    )
    sender.update_account_info()

    recurrent_transfer.execute_future_transfer()

    for execution in range(recurrent_transfer.executions):
        sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1 + 2)

        sender.update_account_info()
        receiver.update_account_info()

        recurrent_transfer.execute_future_transfer()

    # Check balances after the last scheduled recurrent transfer.
    recurrent_transfer.move_after_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount", "executions", "base_recurrence_time", "update_recurrence_time", "offset"),
    [
        # User increases a frequency of defined recurrent transfer in Hive / HBD.
        (
            tt.Asset.Test(10),
            4,
            7 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            2 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            tt.Time.days(2),
        ),
        (
            tt.Asset.Tbd(10),
            4,
            7 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            2 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            tt.Time.days(2),
        ),
        #
        # Timeline:
        #     0d                                  7d         9d         12d        14d        16d        18d
        #     │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│  offset  │‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾│
        # ────■───────────────────────────────────■──────────●──────────●──────────●──────────●──────────●──────────────>[t]
        #     T1E0                                T1E1   (update)       T2E0       T2E1       T2E2       T2E3
        # User decreases a frequency of defined recurrent transfer in Hive / HBD.
        (
            tt.Asset.Test(10),
            4,
            2 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            3 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            tt.Time.days(1),
        ),
        (
            tt.Asset.Tbd(10),
            4,
            2 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            3 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
            tt.Time.days(1),
        ),
        #
        # Timeline:
        #     0d               2d        3d                       6d                       9d                       12d
        #     │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│  offset │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        # ────■────────────────■─────────●────────────────────────●────────────────────────●────────────────────────●───>[t]
        #     T1E0             T1E1   (update)                    T2E0                     T2E1                     T2E2
    ],
)
def test_recurrent_transfer_cases_13_14_15_16(
    node: tt.InitNode,
    wallet: tt.Wallet,
    receiver: RecurrentTransferAccount,
    amount,
    executions,
    base_recurrence_time,
    update_recurrence_time,
    offset,
):
    wallet.create_account(
        "sender",
        hives=6 * amount if isinstance(amount, tt.Asset.TestT) else None,
        hbds=6 * amount if isinstance(amount, tt.Asset.TbdT) else None,
        vests=tt.Asset.Test(1),
    )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=base_recurrence_time,
        executions=executions,
    )
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    sender.rc_manabar.assert_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    # Update recurrence after second transfer and before third one.
    node.restart(time_control=tt.StartTimeControl(start_time=node.get_head_block_time() + offset))
    recurrent_transfer.update(new_recurrence_time=update_recurrence_time)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
    )
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)

    for execution in range(1, recurrent_transfer.executions + 1):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()
        tt.logger.info(f"DATE : {node.get_head_block_time()}, execution: {execution}")

        sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + execution)

        # The time of the third execution of the initial recurrent transfer.
        time_third_execution = recurrent_transfer.executions_schedules[0][2]
        if node.get_head_block_time() < time_third_execution < recurrent_transfer.get_next_execution_date():
            recurrent_transfer.execute_future_transfer(execution_date=recurrent_transfer.executions_schedules[0][2])
            sender.update_account_info()
            receiver.update_account_info()
            sender.assert_hives_and_hbds_are_not_changed()
            receiver.assert_hives_and_hbds_are_not_changed()

    # Check balances after the last scheduled recurrent transfer.
    sender.update_account_info()
    receiver.update_account_info()
    recurrent_transfer.move_after_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount_1", "amount_2", "amount_3", "executions", "first_update_executions", "second_update_executions"),
    [
        (tt.Asset.Test(10), tt.Asset.Test(20), tt.Asset.Test(30), 3, 5, 2),
        (tt.Asset.Tbd(10), tt.Asset.Tbd(20), tt.Asset.Tbd(30), 3, 5, 2),
    ],
)
def test_recurrent_transfer_cases_17_and_18(
    node: tt.InitNode,
    wallet: tt.Wallet,
    receiver: RecurrentTransferAccount,
    sender: RecurrentTransferAccount,
    amount_1: tt.Asset.TestT | tt.Asset.TbdT,
    amount_2: tt.Asset.TestT | tt.Asset.TbdT,
    amount_3: tt.Asset.TestT | tt.Asset.TbdT,
    executions: int,
    first_update_executions: int,
    second_update_executions: int,
):
    """
    User increases a frequency of defined recurrent transfer in Hive / HBD.
    """
    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount_1,
        recurrence=3 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount_1)
    receiver.assert_balance_is_increased_by_transfer(amount_1)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    sender.rc_manabar.assert_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount_1)
    receiver.assert_balance_is_increased_by_transfer(amount_1)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    # Update first time - increase the number of executions and the frequency of recurrent transfers.
    node.restart(time_control=tt.StartTimeControl(start_time=node.get_head_block_time() + tt.Time.days(2)))

    recurrent_transfer.update(
        amount=amount_2,
        new_executions_number=first_update_executions,
        new_recurrence_time=2 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
    )
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)

    for execution in range(recurrent_transfer.executions - 1):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()
        tt.logger.info(f"DATE : {node.get_head_block_time()}, execution: {execution} of first update")

        sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount_2)
        receiver.assert_balance_is_increased_by_transfer(amount_2)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + execution + 1)

    sender.update_account_info()
    receiver.update_account_info()

    node.restart(time_control=tt.StartTimeControl(start_time=node.get_head_block_time() + tt.Time.days(1)))
    assert node.get_head_block_time() < recurrent_transfer.get_next_execution_date()

    # Update second time - decrease the number of executions and the frequency of recurrent transfers.
    recurrent_transfer.update(
        amount=amount_3,
        new_executions_number=second_update_executions,
        new_recurrence_time=4 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=recurrent_transfer.rc_cost, operation_timestamp=recurrent_transfer.timestamp
    )
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + 4)

    for execution in range(recurrent_transfer.executions):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()
        tt.logger.info(f"DATE : {node.get_head_block_time()}, execution: {execution} of second update")

        sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount_3)
        receiver.assert_balance_is_increased_by_transfer(amount_3)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + 4 + execution + 1)

    # Check balances after `recurrence` time.
    sender.update_account_info()
    receiver.update_account_info()
    recurrent_transfer.move_after_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("recurrent_transfer_amount", "transfer_amount"),
    [
        (tt.Asset.Test(10), tt.Asset.Test(20)),
        (tt.Asset.Tbd(10), tt.Asset.Tbd(20)),
    ],
)
def test_recurrent_transfer_cases_19_and_20(
    node: tt.InitNode,
    wallet: tt.Wallet,
    receiver: RecurrentTransferAccount,
    recurrent_transfer_amount: tt.Asset.TestT | tt.Asset.TbdT,
    transfer_amount: tt.Asset.TestT | tt.Asset.TbdT,
):
    """
    User creates a recurrent transfer in Hive / HBD, but the first transfer is not created because the lack of funds.
    """
    wallet.create_account(
        "sender",
        hives=transfer_amount if isinstance(transfer_amount, tt.Asset.TestT) else None,
        hbds=transfer_amount if isinstance(transfer_amount, tt.Asset.TbdT) else None,
        vests=tt.Asset.Test(1),
    )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    with wallet.in_single_transaction() as transaction:
        wallet.api.recurrent_transfer(
            sender.name, receiver.name, recurrent_transfer_amount, "{}", MIN_RECURRENT_TRANSFERS_RECURRENCE, 3
        )
        wallet.api.transfer(sender.name, receiver.name, transfer_amount, "{}")

    transaction_rc_cost = transaction.get_response()["rc_cost"]
    sender.assert_balance_is_reduced_by_transfer(transfer_amount)
    receiver.assert_balance_is_increased_by_transfer(transfer_amount)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=transaction_rc_cost)

    error_message = "Virtual operation - `failed_recurrent_transfer_operation` has not been generated."
    assert len(get_virtual_operations(node, FailedRecurrentTransferOperation)) == 1, error_message

    sender.update_account_info()
    receiver.update_account_info()

    # Check balances after `recurrence` time.
    node.restart(
        time_control=tt.StartTimeControl(
            start_time=node.get_head_block_time() + tt.Time.hours(MIN_RECURRENT_TRANSFERS_RECURRENCE)
        )
    )
    assert len(get_virtual_operations(node, FailedRecurrentTransferOperation)) == 2, error_message
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount", "executions"),
    [
        (tt.Asset.Test(10), 3),
        (tt.Asset.Tbd(10), 3),
    ],
)
def test_recurrent_transfer_cases_21_and_22(
    node: tt.InitNode,
    wallet: tt.Wallet,
    receiver: RecurrentTransferAccount,
    amount: tt.Asset.TestT | tt.Asset.TbdT,
    executions: int,
):
    """
    User creates a recurrent transfer in Hive / HBD with expected second execution to fail (lack of funds).
    """
    wallet.create_account(
        "sender",
        hives=10 if isinstance(amount, tt.Asset.TestT) else None,
        hbds=10 if isinstance(amount, tt.Asset.TbdT) else None,
        vests=tt.Asset.Test(1),
    )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    recurrent_transfer.assert_failed_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    sender.rc_manabar.assert_current_mana_is_unchanged()

    # User receives a transfer and there is enough funds for the next recurrent transfer.
    wallet.api.transfer("initminer", sender.name, amount, "Top up the account for the third recurrent transfer")
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.rc_manabar.assert_current_mana_is_unchanged()
    receiver.assert_balance_is_increased_by_transfer(amount)
    sender.assert_balance_is_reduced_by_transfer(amount)

    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.move_after_last_transfer()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.rc_manabar.assert_current_mana_is_unchanged()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet()
@pytest.mark.parametrize("amount", [(tt.Asset.Test(10)), (tt.Asset.Tbd(10))])
def test_recurrent_transfer_cases_23_and_24(
    node: tt.InitNode, wallet: tt.Wallet, receiver: RecurrentTransferAccount, amount: tt.Asset.TestT | tt.Asset.TbdT
):
    """
    User creates a recurrent transfer in Hive / HBD and the next execution fails
    HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES times, because of the lack of funds.
    """
    wallet.create_account(
        "sender",
        hives=amount if isinstance(amount, tt.Asset.TestT) else None,
        hbds=amount if isinstance(amount, tt.Asset.TbdT) else None,
        vests=tt.Asset.Test(1),
    )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES + 2,
    )

    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)

    for execution in range(MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()

        recurrent_transfer.assert_failed_recurrent_transfer_operation_was_generated(execution + 1)
        sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_hives_and_hbds_are_not_changed()
        receiver.assert_hives_and_hbds_are_not_changed()

    recurrent_transfer.move_after_last_transfer()
    recurrent_transfer.assert_failed_recurrent_transfer_operation_was_generated(
        expected_vop=MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES
    )


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount", "executions", "recurrence"),
    [
        (tt.Asset.Test(10), 3, MAX_RECURRENT_TRANSFER_END_DATE / 2 * 24),
        (tt.Asset.Tbd(10), 3, MAX_RECURRENT_TRANSFER_END_DATE / 2 * 24),
    ],
)
def test_recurrent_transfer_cases_25_and_26(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount: tt.Asset.TestT | tt.Asset.TbdT,
    executions: int,
    recurrence: int,
):
    """
    User creates a recurrent transfer in Hive / HBD to be executed 3 times every year.
    """
    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=recurrence,
        executions=executions,
    )

    for execution in range(executions):
        if execution == 0:
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            sender.rc_manabar.assert_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1)

        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("amount", "executions", "recurrence"),
    [
        (tt.Asset.Test(10), 3, (MAX_RECURRENT_TRANSFER_END_DATE / 2 + 1) * 24),
        (tt.Asset.Tbd(10), 3, (MAX_RECURRENT_TRANSFER_END_DATE / 2 + 1) * 24),
    ],
)
def test_recurrent_transfer_cases_27_and_28(
    node: tt.InitNode,
    wallet: tt.Wallet,
    sender: RecurrentTransferAccount,
    receiver: RecurrentTransferAccount,
    amount: tt.Asset.TestT | tt.Asset.TbdT,
    executions: int,
    recurrence: int,
):
    """
    User tries to create a recurrent transfer in Hive / HBD to be executed 3 times every 366 days.
    """
    with pytest.raises(ErrorInResponseError) as exception:
        RecurrentTransfer(
            node,
            wallet,
            from_=sender.name,
            to=receiver.name,
            amount=amount,
            recurrence=recurrence,
            executions=executions,
        )

    expected_error_message = "Cannot set a transfer that would last for longer than 730 days"
    assert expected_error_message in exception.value.error
