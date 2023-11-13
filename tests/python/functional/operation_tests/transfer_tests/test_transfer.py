from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import get_virtual_operations
from schemas.operations.virtual.dhf_conversion_operation import DhfConversionOperation


@pytest.mark.parametrize(
    ("amount", "memo", "check_balance"),
    [
        # User sends all HBD
        (tt.Asset.Tbd(50), "sending all my hbds", "get_hbd_balance"),
        # User sends all HIVES
        (tt.Asset.Test(50), "sending all my hives", "get_hive_balance"),
        # User sends a transfer in HBD with a memo
        (tt.Asset.Tbd(5), "this is memo", "get_hbd_balance"),
        # User sends a transfer in HIVES with a memo
        (tt.Asset.Test(5), "this is memo", "get_hive_balance"),
        # User sends a transfer in HBD without a memo
        (tt.Asset.Tbd(5), "", "get_hbd_balance"),
        # User sends a transfer in HIVES without a memo
        (tt.Asset.Test(5), "", "get_hive_balance"),
    ],
)
@pytest.mark.testnet()
def test_transfer(alice, bob, prepared_node, wallet, amount, memo, check_balance):
    sender_balance_before_transfer = getattr(alice, check_balance)()
    receiver_balance_before_transfer = getattr(bob, check_balance)()
    rc_amount_before_sending_op = alice.get_rc_current_mana()

    alice.transfer("bob", amount, memo)

    rc_amount_after_sending_op = alice.get_rc_current_mana()
    sender_balance_after_transfer = getattr(alice, check_balance)()
    receiver_balance_after_transfer = getattr(bob, check_balance)()

    assert (
        sender_balance_before_transfer == sender_balance_after_transfer + amount
    ), f"{amount.token} balance of sender wasn't decreased."
    assert (
        receiver_balance_before_transfer == receiver_balance_after_transfer - amount
    ), f"{amount.token} balance of receiver wasn't increased."
    assert (
        rc_amount_before_sending_op > rc_amount_after_sending_op
    ), "RC amount after sending transfer wasn't decreased."


@pytest.mark.testnet()
def test_transfer_hives_to_hive_fund_account(prepared_node, wallet, alice, hive_fund):
    sender_balance_before_sending_hives = alice.get_hive_balance()
    receiver_balance_before_sending_hives = hive_fund.get_hbd_balance()
    rc_amount_before_sending_op = alice.get_rc_current_mana()

    alice.transfer("hive.fund", tt.Asset.Test(5), "")

    assert (
        len(get_virtual_operations(prepared_node, DhfConversionOperation)) == 1
    ), "'dhf_conversion_operation'virtual operation wasn't generated"
    rc_amount_after_sending_op = alice.get_rc_current_mana()
    sender_balance_after_sending_hives = alice.get_hive_balance()
    # hbd balance of hive.fund is checked due to dhf_conversion_operation (hives are converted into hbd when transfer
    # (in hives) to this account is being done)
    receiver_balance_after_sending_hives = hive_fund.get_hbd_balance()

    assert sender_balance_before_sending_hives == sender_balance_after_sending_hives + tt.Asset.Test(
        5
    ), "HIVE balance of sender wasn't decreased."
    assert (
        receiver_balance_before_sending_hives < receiver_balance_after_sending_hives
    ), "HIVE balance of receiver wasn't decreased."
    assert (
        rc_amount_before_sending_op > rc_amount_after_sending_op
    ), "RC amount after sending transfer wasn't decreased."
