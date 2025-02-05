from __future__ import annotations

from typing import TYPE_CHECKING, Iterable

import pytest
from helpy.exceptions import ErrorInResponseError

from hive_local_tools.functional.python.operation import (
    get_number_of_fill_order_operations,
)

if TYPE_CHECKING:
    import test_tools as tt
    from python.functional.operation_tests.conftest import LimitOrderAccount

"""
All test cases are from spreadsheet: https://gitlab.syncad.com/hive/hive/-/issues/485#note_123196
Every test is executed with following parametrization variables:
- use_hbd_in_matching_order - boolean variable that determine whether matching order (purple one) buys HBDS
- create_main_order - function that will be used for creating matching order marked as purple
- create_normal_order - function that will be used for creating all orders (without color) except main

In both cases - create_main_order and create_normal_order operation limit_order_create_operation or
limit_order_create2_operation may be used.
"""
parametrize_variables: tuple[str, str, str] = (
    "use_hbd_in_matching_order",
    "create_main_order",
    "create_normal_order",
)
parametrize_values: Iterable[tuple[bool, str, str]] = (
    (True, "create_order", "create_order"),
    (True, "create_order_2", "create_order_2"),
    (True, "create_order", "create_order_2"),
    (True, "create_order_2", "create_order"),
    (False, "create_order", "create_order"),
    (False, "create_order_2", "create_order_2"),
    (False, "create_order", "create_order_2"),
    (False, "create_order_2", "create_order"),
)


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_not_matching_orders_with_fill_or_kill(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 1, 2 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """
    getattr(alice, create_normal_order)(150, 200, buy_hbd=not use_hbd_in_matching_order)
    with pytest.raises(ErrorInResponseError) as error:
        getattr(bob, create_main_order)(200, 300, buy_hbd=use_hbd_in_matching_order, fill_or_kill=True)

    assert "Cancelling order because it was not filled" in str(error.value), "Other error than expected occurred."


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_match_third_order_with_kill_or_fill(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 3 and 4 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """
    getattr(alice, create_normal_order)(300, 160, buy_hbd=not use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(100, 200, buy_hbd=not use_hbd_in_matching_order)
    getattr(carol, create_main_order)(100, 150, buy_hbd=use_hbd_in_matching_order, fill_or_kill=True)

    alice.assert_not_completed_order(112.5, hbd=use_hbd_in_matching_order)

    check_hbd = (0, 1, 0) if use_hbd_in_matching_order else (1, 0, 1)

    for account, amount, condition in zip((carol, carol, alice), (300, 587.5, 550), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_match_fifth_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    daisy: LimitOrderAccount,
    elizabeth: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 5 and 6 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_normal_order)(100, 150, buy_hbd=not use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(80, 100, buy_hbd=not use_hbd_in_matching_order)
    getattr(carol, create_normal_order)(90, 100, buy_hbd=not use_hbd_in_matching_order)
    getattr(daisy, create_normal_order)(100, 200, buy_hbd=not use_hbd_in_matching_order)
    getattr(elizabeth, create_main_order)(300, 200, buy_hbd=use_hbd_in_matching_order, fill_or_kill=True)

    for account, amount in zip((alice, daisy), (33.334, 100)):
        account.assert_not_completed_order(amount, hbd=use_hbd_in_matching_order)

    check_hbd = (0, 0, 0, 0, 1, 0) if use_hbd_in_matching_order else (1, 1, 1, 1, 0, 1)

    for account, amount, condition in zip(
        (alice, bob, carol, daisy, elizabeth, elizabeth),
        (550, 400, 500, 480, 836.666, 300),
        check_hbd,
    ):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 3
    ), "fill_order_operation virtual operation wasn't generated"


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_not_matching_orders(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 7, 8 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """
    getattr(alice, create_normal_order)(150, 200, buy_hbd=not use_hbd_in_matching_order)
    getattr(bob, create_main_order)(200, 300, buy_hbd=use_hbd_in_matching_order)

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    check_hbd = (1, 0, 1, 0) if use_hbd_in_matching_order else (0, 1, 0, 1)
    for account, amount, condition in zip((alice, alice, bob, bob), (300, 450, 300, 100), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="no_match")
    prepared_node.wait_number_of_blocks(21)  # wait for expiration of orders
    for account, amount, condition in zip((alice, alice, bob, bob), (450, 450, 300, 300), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_full_match_third_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 9, 10 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_normal_order)(300, 160, buy_hbd=not use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(100, 200, buy_hbd=not use_hbd_in_matching_order)
    getattr(carol, create_main_order)(100, 150, buy_hbd=use_hbd_in_matching_order)

    for account, amount in zip((alice, bob), (112.5, 100)):
        account.assert_not_completed_order(amount, hbd=use_hbd_in_matching_order)

    check_hbd = (0, 1, 0) if use_hbd_in_matching_order else (1, 0, 1)
    for account, amount, condition in zip((carol, carol, alice), (300, 587.5, 550), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"

    check_hbd_after_expiration = (0, 1, 1) if use_hbd_in_matching_order else (1, 0, 0)

    prepared_node.wait_number_of_blocks(21)  # wait for expiration of orders
    for account, amount, condition in zip((alice, alice, bob), (550, 262.5, 300), check_hbd_after_expiration):
        account.assert_balance(amount=amount, check_hbd=condition, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_full_match_first_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 11, 12 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_main_order)(150, 100, buy_hbd=use_hbd_in_matching_order)
    alice.assert_balance(amount=300, check_hbd=not use_hbd_in_matching_order, message="creation")

    getattr(bob, create_normal_order)(100, 200, buy_hbd=not use_hbd_in_matching_order)
    # make sure that first order wasn't matched
    alice.assert_balance(amount=450, check_hbd=use_hbd_in_matching_order, message="no_match")
    getattr(carol, create_normal_order)(210, 300, buy_hbd=not use_hbd_in_matching_order)

    for account, amount in zip((bob, carol), (100, 110)):
        account.assert_not_completed_order(amount=amount, hbd=use_hbd_in_matching_order)

    check_hbd = (0, 1, 0) if use_hbd_in_matching_order else (1, 0, 1)
    for account, amount, condition in zip((carol, alice, alice), (550, 550, 300), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")
    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"
    check_hbd_after_expiration = (1, 0, 0) if use_hbd_in_matching_order else (0, 1, 1)
    prepared_node.wait_number_of_blocks(21)  # wait for expiration of all orders
    for account, amount, condition in zip((carol, carol, bob), (300, 550, 300), check_hbd_after_expiration):
        account.assert_balance(amount=amount, check_hbd=condition, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_partial_match_third_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    daisy: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 13, 14 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_normal_order)(300, 160, buy_hbd=use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    getattr(carol, create_main_order)(300, 450, buy_hbd=not use_hbd_in_matching_order)

    check_hbd = (1, 0, 0) if use_hbd_in_matching_order else (0, 1, 1)
    for account, amount, condition in zip((alice, alice, carol), (610, 150, 700), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    for account, amount, condition in zip(
        (bob, carol),
        (100, 140),
        (not use_hbd_in_matching_order, use_hbd_in_matching_order),
    ):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"

    getattr(daisy, create_normal_order)(480, 300, buy_hbd=use_hbd_in_matching_order)
    for account, amount, condition in zip((daisy, carol), (620, 910), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    for account, amount in zip((bob, daisy), (100, 270)):
        account.assert_not_completed_order(amount=amount, hbd=not use_hbd_in_matching_order)

    prepared_node.wait_number_of_blocks(21)  # wait for expiration of all orders
    for account, amount in zip((bob, daisy), (300, 270)):
        account.assert_balance(amount=amount, check_hbd=not use_hbd_in_matching_order, message="expiration")

    assert (
        get_number_of_fill_order_operations(prepared_node) == 2
    ), "fill_order_operation virtual operation wasn't generated"


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_partially_match_every_next_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    daisy: LimitOrderAccount,
    elizabeth: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 15, 16 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_main_order)(450, 300, buy_hbd=not use_hbd_in_matching_order)

    alice.assert_balance(amount=0, check_hbd=use_hbd_in_matching_order, message="creation")

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    getattr(bob, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    alice.assert_balance(amount=450, check_hbd=not use_hbd_in_matching_order, message="no_match")

    getattr(carol, create_normal_order)(100, 120, buy_hbd=use_hbd_in_matching_order)

    for account, amount, condition in zip(
        (alice, bob),
        (300, 100),
        (use_hbd_in_matching_order, not use_hbd_in_matching_order),
    ):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    use_hbd = (1, 0, 0) if use_hbd_in_matching_order else (0, 1, 1)
    for account, amount, condition in zip((carol, carol, alice), (550, 300, 550), use_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"

    getattr(daisy, create_normal_order)(100, 130, buy_hbd=use_hbd_in_matching_order)
    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507

    for account, amount, condition in zip(
        (alice, bob),
        (150, 100),
        (use_hbd_in_matching_order, not use_hbd_in_matching_order),
    ):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip(
        (alice, daisy),
        (650, 630),
        (not use_hbd_in_matching_order, use_hbd_in_matching_order),
    ):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    assert (
        get_number_of_fill_order_operations(prepared_node) == 2
    ), "fill_order_operation virtual operation wasn't generated"
    getattr(elizabeth, create_normal_order)(200, 260, buy_hbd=use_hbd_in_matching_order)

    for account, amount in zip((bob, elizabeth), (100, 100)):
        account.assert_not_completed_order(amount=amount, hbd=not use_hbd_in_matching_order)

    for account, amount, condition in zip(
        (alice, elizabeth),
        (750, 750),
        (not use_hbd_in_matching_order, use_hbd_in_matching_order),
    ):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 3
    ), "fill_order_operation virtual operation wasn't generated"
    prepared_node.wait_number_of_blocks(21)  # wait for expiration of all orders
    for account, amount in zip((bob, elizabeth), (300, 500)):
        account.assert_balance(amount=amount, check_hbd=not use_hbd_in_matching_order, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_match_third_order_partially_and_wait_for_expiration(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 17, 18 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_normal_order)(300, 160, buy_hbd=use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    getattr(carol, create_main_order)(300, 450, buy_hbd=not use_hbd_in_matching_order)

    for account, amount, condition in zip(
        (bob, carol),
        (100, 140),
        (not use_hbd_in_matching_order, use_hbd_in_matching_order),
    ):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip(
        (alice, carol),
        (610, 700),
        (use_hbd_in_matching_order, not use_hbd_in_matching_order),
    ):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"
    prepared_node.wait_number_of_blocks(21)  # wait for expiration of all orders
    carol.assert_balance(amount=240, check_hbd=use_hbd_in_matching_order, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_match_third_order_partially_and_wait_for_expiration_scenario_2(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 19, 20 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_main_order)(450, 300, buy_hbd=not use_hbd_in_matching_order)
    # check if rc was  reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    alice.assert_balance(amount=0, check_hbd=use_hbd_in_matching_order, message="creation")

    getattr(bob, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    alice.assert_balance(amount=450, check_hbd=not use_hbd_in_matching_order, message="no_match")

    getattr(carol, create_normal_order)(100, 120, buy_hbd=use_hbd_in_matching_order)

    for account, amount, condition in zip(
        (alice, bob),
        (300, 100),
        (use_hbd_in_matching_order, not use_hbd_in_matching_order),
    ):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip(
        (alice, carol),
        (550, 550),
        (not use_hbd_in_matching_order, use_hbd_in_matching_order),
    ):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"

    prepared_node.wait_number_of_blocks(21)  # wait for expiration of orders
    alice.assert_balance(amount=300, check_hbd=use_hbd_in_matching_order, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_match_fifth_order_partially(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    daisy: LimitOrderAccount,
    elizabeth: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 21, 22 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_normal_order)(100, 150, buy_hbd=use_hbd_in_matching_order)

    getattr(bob, create_normal_order)(80, 100, buy_hbd=use_hbd_in_matching_order)
    alice.assert_not_completed_order(amount=100, hbd=not use_hbd_in_matching_order)

    getattr(carol, create_normal_order)(90, 100, buy_hbd=use_hbd_in_matching_order)
    alice.assert_not_completed_order(amount=100, hbd=not use_hbd_in_matching_order)

    getattr(daisy, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    alice.assert_not_completed_order(amount=100, hbd=not use_hbd_in_matching_order)

    getattr(elizabeth, create_main_order)(600, 400, buy_hbd=not use_hbd_in_matching_order)
    # check if rc was  reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    elizabeth.assert_not_completed_order(amount=250, hbd=use_hbd_in_matching_order)

    check_hbd = (1, 1, 1, 1, 0) if use_hbd_in_matching_order else (0, 0, 0, 0, 1)
    for account, amount, condition in zip((alice, bob, carol, daisy, elizabeth), (600, 400, 500, 480, 870), check_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    assert (
        get_number_of_fill_order_operations(prepared_node) == 3
    ), "fill_order_operation virtual operation wasn't generated"
    prepared_node.wait_number_of_blocks(21)  # wait for expiration of orders
    elizabeth.assert_balance(amount=250, check_hbd=use_hbd_in_matching_order, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_full_match_fifth_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    daisy: LimitOrderAccount,
    elizabeth: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 23, 24 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_main_order)(400, 600, buy_hbd=not use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    alice.assert_balance(amount=450, check_hbd=not use_hbd_in_matching_order, message="no_match")
    getattr(carol, create_normal_order)(150, 80, buy_hbd=use_hbd_in_matching_order)

    use_hbd_balances = (0, 1) if use_hbd_in_matching_order else (1, 0)
    use_hbd_not_completed_orders = (1, 0) if use_hbd_in_matching_order else (0, 1)

    for account, amount, condition in zip((alice, bob), (300, 100), use_hbd_not_completed_orders):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip((alice, carol), (600, 500), use_hbd_balances):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"

    getattr(daisy, create_normal_order)(150, 90, buy_hbd=use_hbd_in_matching_order)

    for account, amount, condition in zip((alice, bob), (200, 100), use_hbd_not_completed_orders):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip((alice, daisy), (750, 580), use_hbd_balances):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 2
    ), "fill_order_operation virtual operation wasn't generated"
    getattr(elizabeth, create_normal_order)(150, 90, buy_hbd=use_hbd_in_matching_order)

    for account, amount, condition in zip((alice, bob), (100, 100), use_hbd_not_completed_orders):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip((alice, elizabeth), (900, 700), use_hbd_balances):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")
    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 3
    ), "fill_order_operation virtual operation wasn't generated"
    prepared_node.wait_number_of_blocks(21)  # wait for expiration of orders
    alice.assert_balance(amount=150, check_hbd=use_hbd_in_matching_order, message="expiration")


@pytest.mark.parametrize(parametrize_variables, parametrize_values)
@pytest.mark.testnet()
def test_full_match_fourth_order(
    prepared_node: tt.InitNode,
    alice: LimitOrderAccount,
    bob: LimitOrderAccount,
    carol: LimitOrderAccount,
    daisy: LimitOrderAccount,
    use_hbd_in_matching_order: str,
    create_main_order: str,
    create_normal_order: str,
) -> None:
    """
    test case 25, 26 from https://gitlab.syncad.com/hive/hive/-/issues/485 and
    https://gitlab.syncad.com/hive/hive/-/issues/487 and https://gitlab.syncad.com/hive/hive/-/issues/492
    """

    getattr(alice, create_normal_order)(300, 160, buy_hbd=use_hbd_in_matching_order)
    getattr(bob, create_normal_order)(100, 200, buy_hbd=use_hbd_in_matching_order)
    getattr(carol, create_main_order)(400, 600, buy_hbd=not use_hbd_in_matching_order)

    use_hbd = (not use_hbd_in_matching_order, use_hbd_in_matching_order)
    for account, amount, condition in zip((bob, carol), (100, 240), use_hbd):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip(
        (alice, carol),
        (610, 700),
        (use_hbd_in_matching_order, not use_hbd_in_matching_order),
    ):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 1
    ), "fill_order_operation virtual operation wasn't generated"

    getattr(daisy, create_normal_order)(160, 100, buy_hbd=use_hbd_in_matching_order)

    for account, amount, condition in zip((bob, carol), (100, 133.334), use_hbd):
        account.assert_not_completed_order(amount=amount, hbd=condition)

    for account, amount, condition in zip((carol, daisy), (860, 586.666), use_hbd):
        account.assert_balance(amount=amount, check_hbd=condition, message="order_match")

    # check if rc was not reduced after fixing issue: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        get_number_of_fill_order_operations(prepared_node) == 2
    ), "fill_order_operation virtual operation wasn't generated"

    prepared_node.wait_number_of_blocks(21)  # wait for expiration of orders
    carol.assert_balance(amount=133.334, check_hbd=use_hbd_in_matching_order, message="expiration")
