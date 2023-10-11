from __future__ import annotations

import pytest

import test_tools as tt


@pytest.mark.parametrize(
    "receiver, currency, check_savings_balance, check_balance",
    (
        # transfer to savings in HIVES, receiver is the same person as sender
        ("alice", tt.Asset.Test, "get_hive_savings_balance", "get_hive_balance"),
        # transfer to savings in HIVES, receiver is other account
        ("bob", tt.Asset.Test, "get_hive_savings_balance", "get_hive_balance"),
        # transfer to savings in HBDS, receiver is the same person as sender
        ("alice", tt.Asset.Tbd, "get_hbd_savings_balance", "get_hbd_balance"),
        # transfer to savings in HBDS, receiver is other account
        ("bob", tt.Asset.Tbd, "get_hbd_savings_balance", "get_hbd_balance"),
    ),
)
@pytest.mark.testnet
def test_transfer_to_savings_account(
    prepared_node, wallet, alice, bob, receiver, currency, check_savings_balance, check_balance
):
    receiver_account_object = alice if receiver == "alice" else bob
    rc_amount_before_sending_op = alice.get_rc_current_mana()
    sender_balance_before_transfer = getattr(alice, check_balance)()
    receiver_savings_balance_before_transfer = getattr(receiver_account_object, check_savings_balance)()

    wallet.api.transfer_to_savings("alice", receiver, currency(5), "transfer to savings")
    prepared_node.wait_for_irreversible_block()

    prepared_node.restart(time_offset="+45d")
    prepared_node.wait_number_of_blocks(21)
    # Give blockchain chance to trigger interest_operation. There have to be change in savings balance
    # (at first transfer there were no interests to pay out - savings balance was 0)
    wallet.api.transfer_to_savings("alice", receiver, currency(5), "second transfer to savings")

    rc_amount_after_sending_op = alice.get_rc_current_mana()
    sender_balance_after_transfer = getattr(alice, check_balance)()
    receiver_savings_balance_after_transfer = getattr(receiver_account_object, check_savings_balance)()
    interests = prepared_node.api.account_history.enum_virtual_ops(
        block_range_begin=1,
        block_range_end=prepared_node.get_last_block_number() + 1,
        limit=1000,
        include_reversible=True,
        filter=0x000020,
        group_by_block=False,
    )["ops"]

    if currency.token == "TBD":
        assert len(interests) == 1, "interest_operation wasn't generated."
        assert (
            receiver_savings_balance_before_transfer
            == receiver_savings_balance_after_transfer - currency(10) - interests[0]["op"]["value"]["interest"]
        ), "Receiver savings balance wasn't increased by transfers and one interest."
    else:
        assert len(interests) == 0, "interest_operation was generated (it shouldn't be)"
        assert receiver_savings_balance_before_transfer == receiver_savings_balance_after_transfer - currency(
            10
        ), "Receiver savings balance wasn't increased by transfers."

    assert (
        sender_balance_before_transfer - currency(10) == sender_balance_after_transfer
    ), f"{currency.token} balance of sender wasn't decreased."
    assert (
        rc_amount_before_sending_op > rc_amount_after_sending_op
    ), "RC amount after sending transfers wasn't decreased."
