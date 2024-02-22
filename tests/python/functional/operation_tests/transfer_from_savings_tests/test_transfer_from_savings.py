from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    check_if_fill_transfer_from_savings_vop_was_generated,
    get_virtual_operations,
)
from schemas.operations.virtual import FillTransferFromSavingsOperation

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import TransferAccount


@pytest.mark.parametrize(
    ("receiver_of_savings_withdrawal", "currency", "check_savings_balance", "check_balance"),
    [
        # transfer from savings in HIVES, receiver is the same person as sender
        ("alice", tt.Asset.TestT, "get_hive_savings_balance", "get_hive_balance"),
        # transfer from savings in HIVES, receiver is other account
        ("bob", tt.Asset.TestT, "get_hive_savings_balance", "get_hive_balance"),
        # transfer from savings in HBDS, receiver is the same person as sender
        ("alice", tt.Asset.TbdT, "get_hbd_savings_balance", "get_hbd_balance"),
        # transfer from savings in HBDS, receiver is other account
        ("bob", tt.Asset.TbdT, "get_hbd_savings_balance", "get_hbd_balance"),
    ],
)
@pytest.mark.testnet()
def test_transfer_from_savings_account(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    alice: TransferAccount,
    bob: TransferAccount,
    receiver_of_savings_withdrawal: str,
    currency: tt.Asset.AnyT,
    check_balance: str,
    check_savings_balance: str,
):
    receiver_account_object = alice if receiver_of_savings_withdrawal == "alice" else bob
    alice.transfer_to_savings("alice", currency(amount=5), "transfer to savings")

    savings_balance_before_withdrawal = getattr(alice, check_savings_balance)()
    rc_amount_before_sending_op = alice.get_rc_current_mana()

    alice.transfer_from_savings(0, receiver_of_savings_withdrawal, currency(amount=5), "transfer from savings")

    savings_balance_after_withdrawal = getattr(alice, check_savings_balance)()
    rc_amount_after_sending_op = alice.get_rc_current_mana()
    receiver_balance_before_withdrawal = getattr(receiver_account_object, check_balance)()

    assert (
        rc_amount_before_sending_op > rc_amount_after_sending_op
    ), "RC amount after withdrawing hive wasn't decreased."
    assert (
        savings_balance_before_withdrawal - currency(amount=5) == savings_balance_after_withdrawal
    ), f"{currency.token()} savings balance wasn't decreased after withdrawal"
    prepared_node.wait_for_irreversible_block()
    prepared_node.restart(
        time_control=tt.StartTimeControl(start_time=prepared_node.get_head_block_time() + tt.Time.days(3))
    )
    receiver_balance_after_withdrawal = getattr(receiver_account_object, check_balance)()

    assert (
        receiver_balance_before_withdrawal + currency(amount=5) == receiver_balance_after_withdrawal
    ), f"{currency.token()} balance of withdrawal receiver wasn't increased."

    payout_vops = get_virtual_operations(prepared_node, FillTransferFromSavingsOperation)
    assert len(payout_vops) == 1, "fill_transfer_from_savings_operation wasn't generated"


@pytest.mark.parametrize(
    ("currency", "check_savings_balance", "check_balance"),
    [
        # transfers from savings in HIVES
        (tt.Asset.TestT, "get_hive_savings_balance", "get_hive_balance"),
        # transfers from savings in HBDS
        (tt.Asset.TbdT, "get_hbd_savings_balance", "get_hbd_balance"),
    ],
)
@pytest.mark.testnet()
def test_transfer_from_savings_during_few_days(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    alice: TransferAccount,
    currency: tt.Asset.AnyT,
    check_savings_balance: str,
    check_balance: str,
):
    alice.transfer_to_savings("alice", currency(amount=75), "transfer to savings")
    funds_after_transfer_to_savings = getattr(alice, check_balance)()
    amount_to_transfer = currency(amount=0)
    sum_of_completed_transfers = currency(amount=0)
    # This loop iterate through 8 days. During first five days one withdrawal from savings account is being done. Every
    # time for different amount of hives/hbd. After every transfer, amount of RC and savings balance are checked.
    # Additionally starting from day three there are asserts for checking fill_transfer_from_savings virtual operation
    # and verifying hive/hbd balance to make sure that savings withdrawal was completed.
    for day_number in range(8):
        amount_to_transfer += currency(amount=5)
        if day_number < 5:
            transfer_id = day_number
            balance_before_withdrawal = getattr(alice, check_savings_balance)()
            rc_amount_before_sending_op = alice.get_rc_current_mana()
            wallet.api.transfer_from_savings(
                "alice", transfer_id, "alice", amount_to_transfer, f"transfer from savings with id:{transfer_id}"
            )
            rc_amount_after_sending_op = alice.get_rc_current_mana()
            balance_after_withdrawal = getattr(alice, check_savings_balance)()
            prepared_node.wait_for_irreversible_block()
            assert balance_before_withdrawal == balance_after_withdrawal + amount_to_transfer, (
                f"{currency.token()} savings balance wasn't decreased after withdrawal with id:{transfer_id} for amount"
                f" of {amount_to_transfer} {currency.token()}S"
            )
            assert (
                rc_amount_before_sending_op > rc_amount_after_sending_op
            ), f"RC amount after hive withdrawal with id:{transfer_id}  wasn't decreased."
        if day_number >= 3:
            completed_transfer_id = day_number - 3
            # withdrawal needs 3 days to proceed before hives will come into account balance (every next transfer
            # sends 5 hives/hbd more than last one, 3 days -> 15 HIVES/HBD)
            sum_of_completed_transfers += amount_to_transfer - currency(amount=15)

            assert tt.Asset.is_same(
                getattr(alice, check_balance)() - funds_after_transfer_to_savings, sum_of_completed_transfers
            ), f"{currency.token()}S from transfer from savings with id:{completed_transfer_id} didn't arrive."

            assert (
                check_if_fill_transfer_from_savings_vop_was_generated(
                    prepared_node, f"transfer from savings with id:{completed_transfer_id}"
                )
                is True
            ), f"fill_transfer_from_savings from transfer with id:{completed_transfer_id} wasn't generated"

        prepared_node.restart(
            time_control=tt.StartTimeControl(start_time=prepared_node.get_head_block_time() + tt.Time.days(1))
        )
