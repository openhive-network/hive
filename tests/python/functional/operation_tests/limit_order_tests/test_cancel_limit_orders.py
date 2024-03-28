"""
All test are described in https://gitlab.syncad.com/hive/hive/-/issues/493
"""
from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools.functional.python.operation import get_number_of_fill_order_operations, get_virtual_operations
from schemas.operations.virtual.limit_order_cancelled_operation import LimitOrderCancelledOperation

if TYPE_CHECKING:
    import test_tools as tt
    from python.functional.operation_tests.conftest import LimitOrderAccount


@pytest.mark.testnet()
@pytest.mark.parametrize("create_main_order", ["create_order", "create_order_2"])
@pytest.mark.parametrize("use_hbd_in_matching_order", [False, True])
def test_cancel_not_matched_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    create_main_order: str,
    use_hbd_in_matching_order: bool,
) -> None:
    """
    test case 1, 2, 3, 4 from https://gitlab.syncad.com/hive/hive/-/issues/493
    """
    transaction = alice.create_order(200, 400, buy_hbd=use_hbd_in_matching_order)
    alice.check_if_current_rc_mana_was_reduced(transaction)
    alice.assert_balance(amount=250, check_hbd=not use_hbd_in_matching_order, message="creation")

    transaction = getattr(bob, create_main_order)(300, 200, buy_hbd=not use_hbd_in_matching_order, fill_or_kill=False)
    bob.check_if_current_rc_mana_was_reduced(transaction)
    bob.assert_balance(amount=0, check_hbd=use_hbd_in_matching_order, message="no_match")

    transaction = bob.cancel_order()
    bob.check_if_current_rc_mana_was_reduced(transaction)
    bob.assert_balance(amount=300, check_hbd=not use_hbd_in_matching_order, message="cancellation")

    assert (
        len(get_virtual_operations(prepared_node, LimitOrderCancelledOperation)) == 1
    ), "limit_order_cancelled_operation virtual operation wasn't generated"


@pytest.mark.testnet()
@pytest.mark.parametrize("create_main_order", ["create_order", "create_order_2"])
@pytest.mark.parametrize("use_hbd_in_matching_order", [False, True])
def test_cancel_partially_matched_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    create_main_order: str,
    use_hbd_in_matching_order: bool,
) -> None:
    """
    test case 5, 6, 7, 8 from https://gitlab.syncad.com/hive/hive/-/issues/493
    """
    transaction = alice.create_order(300, 160, buy_hbd=use_hbd_in_matching_order)
    alice.check_if_current_rc_mana_was_reduced(transaction)
    alice.assert_balance(amount=150, check_hbd=not use_hbd_in_matching_order, message="creation")

    transaction = bob.create_order(100, 200, buy_hbd=use_hbd_in_matching_order)
    bob.check_if_current_rc_mana_was_reduced(transaction)
    bob.assert_balance(amount=200, check_hbd=not use_hbd_in_matching_order, message="creation")

    transaction = getattr(carol, create_main_order)(300, 450, buy_hbd=not use_hbd_in_matching_order, fill_or_kill=False)
    carol.check_if_current_rc_mana_was_reduced(transaction)
    carol.assert_balance(amount=100, check_hbd=use_hbd_in_matching_order, message="creation")

    carol.assert_balance(amount=700, check_hbd=not use_hbd_in_matching_order, message="order_match")
    alice.assert_balance(amount=610, check_hbd=use_hbd_in_matching_order, message="order_match")
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"
    bob.assert_not_completed_order(100, hbd=not use_hbd_in_matching_order)
    carol.assert_not_completed_order(140, hbd=use_hbd_in_matching_order)

    transaction = carol.cancel_order()
    carol.check_if_current_rc_mana_was_reduced(transaction)
    carol.assert_balance(amount=240, check_hbd=use_hbd_in_matching_order, message="cancellation")
    assert (
        len(get_virtual_operations(prepared_node, LimitOrderCancelledOperation)) == 1
    ), "limit_order_cancelled_operation virtual operation wasn't generated"


@pytest.mark.testnet()
@pytest.mark.parametrize("create_main_order", ["create_order", "create_order_2"])
@pytest.mark.parametrize("use_hbd_in_matching_order", [False, True])
def test_cancel_partially_matched_order_case_2(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    create_main_order: str,
    use_hbd_in_matching_order: bool,
) -> None:
    """
    test case 9, 10, 11, 12 from https://gitlab.syncad.com/hive/hive/-/issues/493
    """
    transaction = getattr(alice, create_main_order)(300, 450, buy_hbd=not use_hbd_in_matching_order, fill_or_kill=False)
    alice.check_if_current_rc_mana_was_reduced(transaction)
    alice.assert_balance(amount=150, check_hbd=use_hbd_in_matching_order, message="creation")

    transaction = bob.create_order(100, 200, buy_hbd=use_hbd_in_matching_order)
    bob.check_if_current_rc_mana_was_reduced(transaction)
    bob.assert_balance(amount=200, check_hbd=not use_hbd_in_matching_order, message="creation")

    transaction = carol.create_order(150, 80, buy_hbd=use_hbd_in_matching_order)
    carol.check_if_current_rc_mana_was_reduced(transaction)
    carol.assert_balance(amount=250, check_hbd=not use_hbd_in_matching_order, message="creation")

    carol.assert_balance(amount=500, check_hbd=use_hbd_in_matching_order, message="order_match")
    alice.assert_balance(amount=600, check_hbd=not use_hbd_in_matching_order, message="order_match")
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"
    bob.assert_not_completed_order(100, hbd=not use_hbd_in_matching_order)
    alice.assert_not_completed_order(200, hbd=use_hbd_in_matching_order)

    transaction = alice.cancel_order()
    alice.check_if_current_rc_mana_was_reduced(transaction)
    alice.assert_balance(amount=350, check_hbd=use_hbd_in_matching_order, message="cancellation")
    assert (
        len(get_virtual_operations(prepared_node, LimitOrderCancelledOperation)) == 1
    ), "limit_order_cancelled_operation virtual operation wasn't generated"
