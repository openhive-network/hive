import pytest

import test_tools as tt
from hive_local_tools.constants import (
    MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES,
    MAX_RECURRENT_TRANSFER_END_DATE,
    MIN_RECURRENT_TRANSFERS_RECURRENCE
)
from hive_local_tools.functional.python.operation import get_virtual_operations
from hive_local_tools.functional.python.operation.recurrent_transfer import RecurrentTransfer, RecurrentTransferAccount


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions", [(tt.Asset.Test(10), 3), (tt.Asset.Tbd(10), 3)])
def test_recurrent_transfer_cases_1_and_2(node, wallet, receiver, amount, executions):
    """
    User creates a recurrent transfer in Hive / HBD to be sent once a day for three days
    """
    wallet.create_account("sender",
                          hives=executions * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=executions * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1)
                          )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    for execution in range(executions):
        if execution == 0:
            recurrent_transfer = RecurrentTransfer(node, wallet,
                                                   from_=sender.name,
                                                   to=receiver.name,
                                                   amount=amount,
                                                   recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
                                                   executions=executions)
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            node.wait_for_irreversible_block()
            recurrent_transfer.execute_future_transfer()
            sender.rc_manabar.assert_rc_current_mana_is_unchanged()
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


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions", [(tt.Asset.Test(10), 3), (tt.Asset.Tbd(10), 3)])
def test_recurrent_transfer_cases_3_and_4(node, wallet, receiver, amount, executions):
    """
    User removes a defined recurrent transfer in Hive / HBD.
    """

    wallet.create_account("sender",
                          hives=executions * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=executions * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
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

    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    cancel_recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=tt.Asset.Test(0) if isinstance(amount, tt.Asset.Test) else tt.Asset.Tbd(0),
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions,
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=cancel_recurrent_transfer.rc_cost,
                                                        operation_timestamp=cancel_recurrent_transfer.timestamp)
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    cancel_recurrent_transfer.execute_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet
@pytest.mark.parametrize("amount_1, amount_2, executions_1, executions_2", [
    (tt.Asset.Test(10), tt.Asset.Test(20), 4, 2),  # User increases an amount of defined recurrent transfer in Hive.
    (tt.Asset.Tbd(10), tt.Asset.Tbd(20), 4, 2),  # User increases an amount of defined recurrent transfer in HBD.
    (tt.Asset.Test(20), tt.Asset.Test(10), 4, 2),  # User decreases an amount of defined recurrent transfer in Hive.
    (tt.Asset.Tbd(20), tt.Asset.Tbd(10), 4, 2),  # User decreases an amount of defined recurrent transfer in HBD.
])
def test_recurrent_transfer_cases_5_6_7_8(node, wallet, receiver, amount_1, amount_2, executions_1, executions_2):
    wallet.create_account("sender",
                          hives=executions_1 * amount_1 + amount_2 if isinstance(amount_1, tt.Asset.Test) else None,
                          hbds=executions_1 * amount_1 + amount_2 if isinstance(amount_1, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    for execution in range(executions_1 // 2):
        if execution == 0:
            recurrent_transfer = RecurrentTransfer(node, wallet,
                                                   from_=sender.name,
                                                   to=receiver.name,
                                                   amount=amount_1,
                                                   recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
                                                   executions=executions_1)

            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            recurrent_transfer.execute_future_transfer()
            sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount_1)
        receiver.assert_balance_is_increased_by_transfer(amount_1)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1)

        sender.update_account_info()
        receiver.update_account_info()

    for execution in range(executions_2):
        if execution == 0:
            recurrent_transfer.update(amount=amount_2, new_executions_number=executions_2)
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost,
                                                                operation_timestamp=recurrent_transfer.timestamp)
            sender.assert_hives_and_hbds_are_not_changed()
            receiver.assert_hives_and_hbds_are_not_changed()
            recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
        else:
            recurrent_transfer.execute_future_transfer()
            sender.rc_manabar.assert_rc_current_mana_is_unchanged()
            sender.assert_balance_is_reduced_by_transfer(amount_2)
            receiver.assert_balance_is_increased_by_transfer(amount_2)
            recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 2)
        sender.update_account_info()
        receiver.update_account_info()

    recurrent_transfer.move_after_last_transfer()
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    receiver.rc_manabar.assert_rc_current_mana_is_unchanged()


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions, update_executions", [
    (tt.Asset.Test(10), 3, 5),
    (tt.Asset.Tbd(10), 3, 5),
])
def test_recurrent_transfer_cases_9_and_10(node, wallet, receiver, amount, executions, update_executions):
    """
    User increases a number of defined recurrent transfer in Hive / HBD.
    """
    wallet.create_account("sender",
                          hives=8 * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=8 * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(node, wallet,
                                           from_=sender.name,
                                           to=receiver.name,
                                           amount=amount,
                                           recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
                                           executions=executions)

    for execution in range(recurrent_transfer.executions - 1):
        if execution == 0:
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
        else:
            sender.rc_manabar.assert_rc_current_mana_is_unchanged()
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
            sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost,
                                                                operation_timestamp=recurrent_transfer.timestamp)
        else:
            sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1 + 2)

        sender.update_account_info()
        receiver.update_account_info()

        recurrent_transfer.execute_future_transfer()

    recurrent_transfer.move_after_last_transfer()

    if isinstance(amount, tt.Asset.Test):
        assert sender.hive == tt.Asset.Test(10)
        # 2 executions from first transfer and 5 executions from second transfer
        assert receiver.hive == (2 + 5) * amount
    if isinstance(amount, tt.Asset.Tbd):
        assert sender.hbd == tt.Asset.Tbd(10)
        # 2 executions from first transfer and 5 executions from second transfer
        assert receiver.hbd == (2 + 5) * amount


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions, update_executions", [
    (tt.Asset.Test(10), 4, 3),
    (tt.Asset.Tbd(10), 4, 3),
])
def test_recurrent_transfer_cases_11_and_12(node, wallet, receiver, amount, executions, update_executions):
    """
    User decreases a number of defined recurrent transfer in Hive / HBD.
    """
    wallet.create_account("sender",
                          hives=5 * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=5 * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(node, wallet,
                                           from_=sender.name,
                                           to=receiver.name,
                                           amount=amount,
                                           recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
                                           executions=executions)

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    # After 2 of 4 executions - user decrease to 3 the number of execution in an existing recurrent transfer.
    recurrent_transfer.update(new_executions_number=update_executions)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost,
                                                        operation_timestamp=recurrent_transfer.timestamp)
    sender.update_account_info()

    recurrent_transfer.execute_future_transfer()

    for execution in range(recurrent_transfer.executions):
        sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1 + 2)

        sender.update_account_info()
        receiver.update_account_info()

        recurrent_transfer.execute_future_transfer()

    # Check balances after the last scheduled recurring transfer.
    recurrent_transfer.move_after_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions, base_recurrence_time, update_recurrence_time, offset", [
    # User increases a frequency of defined recurrent transfer in Hive / HBD.
    (tt.Asset.Test(10), 4, 7*MIN_RECURRENT_TRANSFERS_RECURRENCE, 2*MIN_RECURRENT_TRANSFERS_RECURRENCE, tt.Time.days(2)),
    (tt.Asset.Tbd(10), 4, 7*MIN_RECURRENT_TRANSFERS_RECURRENCE, 2*MIN_RECURRENT_TRANSFERS_RECURRENCE, tt.Time.days(2)),
    #
    # Timeline:
    #     0d                                  7d         9d         12d        14d        16d        18d
    #     │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│  offset  │‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾│
    # ────■───────────────────────────────────■──────────●──────────●──────────●──────────●──────────●──────────────>[t]
    #     T1E0                                T1E1   (update)       T2E0       T2E1       T2E2       T2E3


    # User decreases a frequency of defined recurrent transfer in Hive / HBD.
    (tt.Asset.Test(10), 4, 2*MIN_RECURRENT_TRANSFERS_RECURRENCE, 3*MIN_RECURRENT_TRANSFERS_RECURRENCE, tt.Time.days(1)),
    (tt.Asset.Tbd(10), 4, 2*MIN_RECURRENT_TRANSFERS_RECURRENCE, 3*MIN_RECURRENT_TRANSFERS_RECURRENCE, tt.Time.days(1)),
    #
    # Timeline:
    #     0d               2d        3d                       6d                       9d                       12d
    #     │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│  offset │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
    # ────■────────────────■─────────●────────────────────────●────────────────────────●────────────────────────●───>[t]
    #     T1E0             T1E1   (update)                    T2E0                     T2E1                     T2E2
])
def test_recurrent_transfer_cases_13_14_15_16(node, wallet, receiver, amount, executions, base_recurrence_time,
                                              update_recurrence_time, offset):
    wallet.create_account("sender",
                          hives=6 * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=6 * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=base_recurrence_time,
        executions=executions
    )
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount)
    receiver.assert_balance_is_increased_by_transfer(amount)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    # Update recurrence after second transfer and before third one.
    node.restart(time_offset=tt.Time.serialize(node.get_head_block_time() + offset, format_=tt.Time.TIME_OFFSET_FORMAT))
    recurrent_transfer.update(new_recurrence_time=update_recurrence_time)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost,
                                                        operation_timestamp=recurrent_transfer.timestamp)
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)

    for execution in range(1, recurrent_transfer.executions + 1):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()
        tt.logger.info(f"DATE : {node.get_head_block_time()}, execution: {execution}")

        sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + execution)

        # The time of the third execution of the base recurrent transfer.
        time_third_execution = recurrent_transfer.executions_schedules[0][2]
        if node.get_head_block_time() < time_third_execution < recurrent_transfer.get_next_execution_date():
            recurrent_transfer.execute_future_transfer(execution_date=recurrent_transfer.executions_schedules[0][2])
            sender.update_account_info()
            receiver.update_account_info()
            sender.assert_hives_and_hbds_are_not_changed()
            receiver.assert_hives_and_hbds_are_not_changed()

    # Check balances after the last scheduled recurring transfer.
    sender.update_account_info()
    receiver.update_account_info()
    recurrent_transfer.move_after_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet
@pytest.mark.parametrize("amount_1, amount_2, amount_3, executions, first_update_executions, second_update_executions", [
    (tt.Asset.Test(10), tt.Asset.Test(20), tt.Asset.Test(30), 3, 5, 2),
    (tt.Asset.Tbd(10), tt.Asset.Tbd(20), tt.Asset.Tbd(30), 3, 5, 2),
])
def test_recurrent_transfer_cases_17_and_18(node, wallet, receiver, amount_1, amount_2, amount_3, executions,
                                            first_update_executions, second_update_executions):
    """
    User increases a frequency of defined recurrent transfer in Hive / HBD.
    """
    wallet.create_account("sender",
                          hives=160 if isinstance(amount_1, tt.Asset.Test) else None,
                          hbds=160 if isinstance(amount_1, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount_1,
        recurrence=3 * MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions
    )

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost)
    sender.assert_balance_is_reduced_by_transfer(amount_1)
    receiver.assert_balance_is_increased_by_transfer(amount_1)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=1)
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    sender.assert_balance_is_reduced_by_transfer(amount_1)
    receiver.assert_balance_is_increased_by_transfer(amount_1)
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.update_account_info()
    receiver.update_account_info()

    # Update first time - increase the number of executions and the frequency of recurrent transfers.
    node.restart(
        time_offset=tt.Time.serialize(node.get_head_block_time() + tt.Time.days(2), format_=tt.Time.TIME_OFFSET_FORMAT))

    recurrent_transfer.update(amount=amount_2, new_executions_number=first_update_executions,
                              new_recurrence_time=2 * MIN_RECURRENT_TRANSFERS_RECURRENCE)

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost,
                                                        operation_timestamp=recurrent_transfer.timestamp)
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)

    for execution in range(recurrent_transfer.executions - 1):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()
        tt.logger.info(f"DATE : {node.get_head_block_time()}, execution: {execution} of first update")

        sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount_2)
        receiver.assert_balance_is_increased_by_transfer(amount_2)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + execution + 1)

    # Update second time - decrease the number of executions and the frequency of recurrent transfers.
    sender.update_account_info()
    receiver.update_account_info()

    node.restart(
        time_offset=tt.Time.serialize(node.get_head_block_time() + tt.Time.days(1), format_=tt.Time.TIME_OFFSET_FORMAT))
    assert node.get_head_block_time() < recurrent_transfer.get_next_execution_date()

    recurrent_transfer.update(amount=amount_3, new_executions_number=second_update_executions,
                              new_recurrence_time=4 * MIN_RECURRENT_TRANSFERS_RECURRENCE)

    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=recurrent_transfer.rc_cost,
                                                        operation_timestamp=recurrent_transfer.timestamp)
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + 4)

    for execution in range(recurrent_transfer.executions):
        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()
        tt.logger.info(f"DATE : {node.get_head_block_time()}, execution: {execution} of second update")

        sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount_3)
        receiver.assert_balance_is_increased_by_transfer(amount_3)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2 + 4 + execution + 1)

    # Check balances after `recurrence` time.
    sender.update_account_info()
    receiver.update_account_info()
    recurrent_transfer.move_after_last_transfer()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet
@pytest.mark.parametrize("recurrent_transfer_amount, transfer_amount", [
    (tt.Asset.Test(10), tt.Asset.Test(20)),
    (tt.Asset.Tbd(10), tt.Asset.Tbd(20)),
])
def test_recurrent_transfer_cases_19_and_20(node, wallet, receiver, recurrent_transfer_amount, transfer_amount):
    """
    User creates a recurrent transfer in Hive / HBD, but the first transfer is not created because the lack of funds.
    """
    wallet.create_account("sender",
                          hives=20 if isinstance(transfer_amount, tt.Asset.Test) else None,
                          hbds=20 if isinstance(transfer_amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    with wallet.in_single_transaction() as transaction:
        wallet.api.recurrent_transfer(sender.name, receiver.name, recurrent_transfer_amount, "{}",
                                      MIN_RECURRENT_TRANSFERS_RECURRENCE, 3)
        wallet.api.transfer(sender.name, receiver.name, transfer_amount, "{}")

    transaction_rc_cost = transaction.get_response()["rc_cost"]
    sender.assert_balance_is_reduced_by_transfer(transfer_amount)
    receiver.assert_balance_is_increased_by_transfer(transfer_amount)
    sender.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=transaction_rc_cost)

    error_message = "Virtual operation - `failed_recurrent_transfer_operation` has not been generated."
    assert len(get_virtual_operations(node, "failed_recurrent_transfer_operation")) == 1, error_message

    sender.update_account_info()
    receiver.update_account_info()

    # Check balances after `recurrence` time.
    node.restart(
        time_offset=tt.Time.serialize(node.get_head_block_time() + tt.Time.hours(MIN_RECURRENT_TRANSFERS_RECURRENCE),
                                      format_=tt.Time.TIME_OFFSET_FORMAT))
    assert len(get_virtual_operations(node, "failed_recurrent_transfer_operation")) == 2, error_message
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions", [
    (tt.Asset.Test(10), 3),
    (tt.Asset.Tbd(10), 3),
])
def test_recurrent_transfer_cases_21_and_22(node, wallet, receiver, amount, executions):
    """
    User creates a recurrent transfer in Hive / HBD and the second recurrent transfer failed once,
    because of the lack of funds.
    """
    wallet.create_account("sender",
                          hives=10 if isinstance(amount, tt.Asset.Test) else None,
                          hbds=10 if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    recurrent_transfer = RecurrentTransfer(
        node,
        wallet,
        from_=sender.name,
        to=receiver.name,
        amount=amount,
        recurrence=MIN_RECURRENT_TRANSFERS_RECURRENCE,
        executions=executions
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
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()

    # User receives a transfer and there is enough funds for the next recurrent transfer.
    wallet.api.transfer("initminer", sender.name, amount, "Top up the account for the third recurring transfer")
    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.execute_future_transfer()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    receiver.assert_balance_is_increased_by_transfer(amount)
    sender.assert_balance_is_reduced_by_transfer(amount)

    sender.update_account_info()
    receiver.update_account_info()

    recurrent_transfer.move_after_last_transfer()
    recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=2)
    sender.rc_manabar.assert_rc_current_mana_is_unchanged()
    sender.assert_hives_and_hbds_are_not_changed()
    receiver.assert_hives_and_hbds_are_not_changed()


@pytest.mark.testnet
@pytest.mark.parametrize("amount", [(tt.Asset.Test(10)), (tt.Asset.Tbd(10))])
def test_recurrent_transfer_cases_23_and_24(node, wallet, receiver, amount):
    """
    User creates a recurrent transfer in Hive / HBD and the execution of the next recurrent transfer
    fails HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES times, because of the lack of funds.
    """
    wallet.create_account("sender",
                          hives=amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
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
        sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_hives_and_hbds_are_not_changed()
        receiver.assert_hives_and_hbds_are_not_changed()

    recurrent_transfer.move_after_last_transfer()
    recurrent_transfer.assert_failed_recurrent_transfer_operation_was_generated(
        expected_vop=MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES)


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions, recurrence", [
    (tt.Asset.Test(10), 3, MAX_RECURRENT_TRANSFER_END_DATE / 2 * 24),
    (tt.Asset.Tbd(10), 3, MAX_RECURRENT_TRANSFER_END_DATE / 2 * 24),
])
def test_recurrent_transfer_cases_25_and_26(node, wallet, receiver, amount, executions, recurrence):
    """
    User creates a recurrent transfer in Hive / HBD to be executed 3 times every year.
    """
    wallet.create_account("sender",
                          hives=executions * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=executions * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

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
            sender.rc_manabar.assert_rc_current_mana_is_unchanged()
        sender.assert_balance_is_reduced_by_transfer(amount)
        receiver.assert_balance_is_increased_by_transfer(amount)
        recurrent_transfer.assert_fill_recurrent_transfer_operation_was_generated(expected_vop=execution + 1)

        sender.update_account_info()
        receiver.update_account_info()
        recurrent_transfer.execute_future_transfer()


@pytest.mark.testnet
@pytest.mark.parametrize("amount, executions, recurrence", [
    (tt.Asset.Test(10), 3, (MAX_RECURRENT_TRANSFER_END_DATE / 2 + 1) * 24),
    (tt.Asset.Tbd(10), 3, (MAX_RECURRENT_TRANSFER_END_DATE / 2 + 1) * 24),
])
def test_recurrent_transfer_cases_27_and_28(node, wallet, receiver, amount, executions, recurrence):
    """
    User tries to create a recurrent transfer in Hive / HBD to be executed 3 times every 366 days.
    """
    wallet.create_account("sender",
                          hives=executions * amount if isinstance(amount, tt.Asset.Test) else None,
                          hbds=executions * amount if isinstance(amount, tt.Asset.Tbd) else None,
                          vests=tt.Asset.Test(1))
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
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
    assert expected_error_message in exception.value.response["error"]["message"]
