"""
All test cases are described in https://gitlab.syncad.com/hive/hive/-/issues/478
Jumping in time by faketimelib is not possible in these tests because HIVE_CONVERSION_DELAY is connected with block
number - not it's timestamp.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    generate_large_amount_of_blocks,
    publish_feeds,
)

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import ConvertAccount


@pytest.mark.testnet()
def test_convert_hbd_to_hive(
    create_node_and_wallet_for_convert_tests: tuple[tt.InitNode, tt.Wallet], alice: ConvertAccount
) -> None:
    # test case 1 from: https://gitlab.syncad.com/hive/hive/-/issues/478
    node, wallet = create_node_and_wallet_for_convert_tests
    publish_feeds(node, wallet, base=1000, quote=400)
    node.api.debug_node.debug_generate_blocks(  # wait 2 blocks for applying feeds
        debug_key=tt.Account("initminer").private_key,
        count=2,
        skip=0,
        miss_blocks=0,
    )
    transaction = alice.convert_hbd(tt.Asset.Tbd(50))
    alice.check_if_current_rc_mana_was_reduced(transaction)
    alice.check_if_funds_to_convert_were_subtracted(transaction)
    # generate enough block to jump HIVE_COLLATERALIZED_CONVERSION_DELAY interval
    generate_large_amount_of_blocks(node=node, block_signer=tt.Account("initminer"), count=1680)
    alice.check_if_right_amount_was_converted_and_added_to_balance(transaction)
    alice.assert_fill_convert_request_operation(expected_amount=1)


@pytest.mark.testnet()
def test_convert_hbd_to_hive_during_few_days(
    create_node_and_wallet_for_convert_tests: tuple, alice: ConvertAccount
) -> None:
    """
    test case 2 from: https://gitlab.syncad.com/hive/hive/-/issues/478
    C1, C2, C3, C4, C5 - convert operations for different values
    Hives from C1, C2, C3, C4, C5 - time when hives converted from hbds are added to account balance (after
    HIVE_CONVERSION_DELAY)
    Timeline:                                            ──────────HIVE_CONVERSION_DELAY──────────
                                      ──────────────HIVE_CONVERSION_DELAY───────────────
                            ──────────────HIVE_CONVERSION_DELAY───────────────
                  ──────────────HIVE_CONVERSION_DELAY───────────────
        0h────────HIVE_CONVERSION_DELAY────────84m
        │‾‾20min‾‾│‾‾20min‾‾│‾‾20min‾‾│‾‾24min‾‾│‾‾10min‾‾│‾‾10min‾‾│‾‾20min‾‾│‾‾20min‾‾│‾‾34min‾‾│
    ────■─────────■─────────■─────────■─────────●─────────■─────────●─────────●─────────●─────────●────────────>[t]
        ↓         ↓         ↓         ↓         ↓         ↓         ↓         ↓         ↓         ↓
      Create   Create    Create    Create     Hives     Create    Hives     Hives     Hives     Hives
        C1       C2        C3        C4       from C1     C5      from C2   from C3   from C4  from C5
    """
    node, wallet = create_node_and_wallet_for_convert_tests
    convert_transactions = []
    vops_counter = 0
    convert_transactions_it = 0
    convert_amount = 5

    publish_feeds(node, wallet, base=1000, quote=400)
    node.api.debug_node.debug_generate_blocks(  # wait 2 blocks for applying feeds
        debug_key=tt.Account("initminer").private_key,
        count=2,
        skip=0,
        miss_blocks=0,
    )

    for iterations, blocks_to_generate in zip(range(10), (400, 400, 400, 480, 200, 200, 400, 400, 680)):
        if iterations in (0, 1, 2, 3, 5):
            transaction = alice.convert_hbd(tt.Asset.Tbd(convert_amount), broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=transaction)
            convert_amount += 5
            convert_transactions.append(transaction)
            alice.check_if_funds_to_convert_were_subtracted(transaction)

        else:
            vops_counter += 1
            alice.check_if_right_amount_was_converted_and_added_to_balance(
                convert_transactions[convert_transactions_it]
            )
            alice.assert_fill_convert_request_operation(expected_amount=vops_counter)
            convert_transactions_it += 1

        generate_large_amount_of_blocks(node, tt.Account("initminer"), count=blocks_to_generate)


@pytest.mark.testnet()
def test_convert_hive_to_hbd(
    create_node_and_wallet_for_convert_tests: tuple[tt.InitNode, tt.Wallet], alice: ConvertAccount
) -> None:
    # test case 1 from: https://gitlab.syncad.com/hive/hive/-/issues/481
    node, wallet = create_node_and_wallet_for_convert_tests
    publish_feeds(node, wallet, base=1000, quote=400)
    node.api.debug_node.debug_generate_blocks(  # wait 2 blocks for applying feeds
        debug_key=tt.Account("initminer").private_key,
        count=2,
        skip=0,
        miss_blocks=0,
    )
    transaction = alice.convert_hives(tt.Asset.Test(200))

    alice.assert_collateralized_convert_immediate_conversion_operation(expected_amount=1)
    alice.check_if_current_rc_mana_was_reduced(transaction)

    alice.assert_account_balance_after_creating_collateral_conversion_request(transaction)
    alice.assert_collateralized_conversion_requests(trx=transaction, state="create")

    # generate enough blocks to jump HIVE_COLLATERALIZED_CONVERSION_DELAY interval
    generate_large_amount_of_blocks(node=node, block_signer=tt.Account("initminer"), count=1680)
    alice.assert_if_right_amount_of_hives_came_back_after_collateral_conversion(transaction)
    alice.assert_collateralized_conversion_requests(trx=transaction, state="delete")
    alice.assert_fill_collateralized_convert_request_operation(expected_amount=1)


@pytest.mark.testnet()
def test_convert_hive_to_hbd_during_few_days(
    create_node_and_wallet_for_convert_tests: tuple, alice: ConvertAccount
) -> None:
    """
    test case 2 from: https://gitlab.syncad.com/hive/hive/-/issues/481
    This test case is very similar to test_convert_hbd_to_hive_during_few_days but collateral_convert and few more
    checks are being done.
    """

    node, wallet = create_node_and_wallet_for_convert_tests
    convert_transactions = []
    vops_counter = 0
    convert_transactions_it = 0
    convert_amount = 5

    publish_feeds(node, wallet, base=1000, quote=400)
    node.api.debug_node.debug_generate_blocks(  # wait 2 blocks for applying feeds
        debug_key=tt.Account("initminer").private_key,
        count=2,
        skip=0,
        miss_blocks=0,
    )

    for iteration, blocks_to_generate in zip(range(9), (400, 400, 400, 480, 200, 200, 400, 400, 680)):
        if iteration in (0, 1, 2, 3, 5):
            transaction = alice.convert_hives(tt.Asset.Test(convert_amount), broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=transaction)
            convert_amount += 5
            convert_transactions.append(transaction)
            alice.assert_collateralized_conversion_requests(trx=transaction, state="create")
            alice.assert_account_balance_after_creating_collateral_conversion_request(transaction)
            node.api.debug_node.debug_generate_blocks(
                debug_key=tt.Account("initminer").private_key,
                count=2,
                skip=0,
                miss_blocks=0,
            )
            alice.assert_collateralized_convert_immediate_conversion_operation(
                expected_amount=len(convert_transactions)
            )
        else:
            vops_counter += 1
            alice.assert_if_right_amount_of_hives_came_back_after_collateral_conversion(
                convert_transactions[convert_transactions_it]
            )

            alice.assert_collateralized_conversion_requests(
                trx=convert_transactions[convert_transactions_it], state="delete"
            )
            alice.assert_fill_collateralized_convert_request_operation(expected_amount=vops_counter)
            convert_transactions_it += 1
        generate_large_amount_of_blocks(node, tt.Account("initminer"), count=blocks_to_generate)


@pytest.mark.parametrize(
    ("initial_feed_base_value", "further_feed_base_value"),
    [
        (  # Minimal feed price < Median feed price after HIVE_COLLATERALIZED_CONVERSION_DELAY, 1 HBD < 1 HIVE
            400,
            404,
        ),
        (  # Minimal feed price > Median feed price after HIVE_COLLATERALIZED_CONVERSION_DELAY,  1 HBD < 1 HIVE
            404,
            400,
        ),
        (  # Minimal feed price < Median feed price after HIVE_COLLATERALIZED_CONVERSION_DELAY,  1 HBD > 1 HIVE
            1400,
            1404,
        ),
        (  # Minimal feed price > Median feed price after HIVE_COLLATERALIZED_CONVERSION_DELAY,  1 HBD > 1 HIVE
            1404,
            1400,
        ),
        (  # Minimal feed price > Median feed price after HIVE_COLLATERALIZED_CONVERSION_DELAY,
            # Not enough HIVE on the collateral balance
            1404,
            600,
        ),
    ],
)
def test_convert_hive_into_hbd_with_changing_median_current_price_during_conversion(
    create_node_and_wallet_for_convert_tests: tuple[tt.InitNode, tt.Wallet],
    alice: ConvertAccount,
    initial_feed_base_value: int,
    further_feed_base_value: int,
) -> None:
    # test case from Excel file (sheet TC1) https://gitlab.syncad.com/hive/hive/-/issues/481#note_154312
    node, wallet = create_node_and_wallet_for_convert_tests
    publish_feeds(node, wallet, initial_feed_base_value, 1000)

    node.api.debug_node.debug_generate_blocks(
        debug_key=tt.Account("initminer").private_key, count=200, skip=0, miss_blocks=0
    )
    transaction = alice.convert_hives(tt.Asset.Test(200))
    alice.check_if_current_rc_mana_was_reduced(transaction)
    alice.assert_account_balance_after_creating_collateral_conversion_request(transaction)
    alice.assert_collateralized_convert_immediate_conversion_operation(expected_amount=1)
    alice.assert_collateralized_conversion_requests(trx=transaction, state="create")

    publish_feeds(node, wallet, further_feed_base_value, 1000)

    # generate enough block to jump HIVE_COLLATERALIZED_CONVERSION_DELAY interval
    generate_large_amount_of_blocks(node=node, block_signer=tt.Account("initminer"), count=1680)
    alice.assert_if_right_amount_of_hives_came_back_after_collateral_conversion(transaction)
    alice.assert_collateralized_conversion_requests(trx=transaction, state="delete")
    alice.assert_fill_collateralized_convert_request_operation(expected_amount=1)


@pytest.mark.parametrize(
    "feed_base_values",
    [
        (404, 402, 400, 398, 396, 390),  # Minimal feed price > Median feed price after 3.5 days,  1HBD < 1 HIVE
        (1404, 1402, 1400, 1398, 1396, 1390),  # Minimal feed price > Median feed price after 3.5 days,  1HBD > 1 HIVE
    ],
)
def test_minimal_feed_price_greater_than_median_feed_price(
    create_node_and_wallet_for_convert_tests: tuple[tt.InitNode, tt.Wallet],
    alice: ConvertAccount,
    feed_base_values: tuple,
) -> None:
    # test case from Excel file (sheet TC2 - columns D, F) https://gitlab.syncad.com/hive/hive/-/issues/481#note_154312
    node, wallet = create_node_and_wallet_for_convert_tests
    convert_transactions = []
    vops_counter = 0
    convert_transactions_it = 0
    convert_amount = 5
    feeds_it = iter(feed_base_values)

    for iteration, blocks_to_generate in zip(range(9), (200, 200, 400, 1080, 200, 400, 1280, 400, 2)):
        if iteration in (4, 5, 6, 7, 8):  # checking if convert requests were completed
            vops_counter += 1
            alice.assert_if_right_amount_of_hives_came_back_after_collateral_conversion(
                convert_transactions[convert_transactions_it]
            )

            alice.assert_collateralized_conversion_requests(
                trx=convert_transactions[convert_transactions_it], state="delete"
            )
            alice.assert_fill_collateralized_convert_request_operation(expected_amount=vops_counter)
            convert_transactions_it += 1
        if iteration in (1, 2, 3, 5, 6):  # creating convert requests
            transaction = alice.convert_hives(tt.Asset.Test(convert_amount), broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=transaction)
            convert_amount += 5
            convert_transactions.append(transaction)
            alice.assert_collateralized_conversion_requests(trx=transaction, state="create")
            alice.assert_account_balance_after_creating_collateral_conversion_request(transaction)
            node.api.debug_node.debug_generate_blocks(
                debug_key=tt.Account("initminer").private_key,
                count=2,
                skip=0,
                miss_blocks=0,
            )
            alice.assert_collateralized_convert_immediate_conversion_operation(
                expected_amount=len(convert_transactions)
            )
        if iteration in (0, 1, 2, 3, 5, 6):  # setting up feed prices
            trx = publish_feeds(node, wallet, next(feeds_it), 1000, broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=trx)

        generate_large_amount_of_blocks(node=node, block_signer=tt.Account("initminer"), count=blocks_to_generate)


@pytest.mark.parametrize(
    "feed_base_values",
    [
        (400, 402, 404, 406, 408, 410),  # Minimal feed price < Median feed price after 3.5 days,  1HBD < 1 HIVE
        (1400, 1402, 1404, 1406, 1408, 1410),  # Minimal feed price < Median feed price after 3.5 days,  1HBD > 1 HIVE
    ],
)
def test_median_feed_price_greater_than_minimal_feed_price(
    create_node_and_wallet_for_convert_tests: tuple[tt.InitNode, tt.Wallet],
    alice: ConvertAccount,
    feed_base_values: tuple,
) -> None:
    # test case from Excel file (sheet TC2 - columns C, E) https://gitlab.syncad.com/hive/hive/-/issues/481#note_154312
    node, wallet = create_node_and_wallet_for_convert_tests
    convert_transactions = []
    vops_counter = 0
    convert_transactions_it = 0
    convert_amount = 5
    feeds_it = iter(feed_base_values)

    for iteration, blocks_to_generate in zip(
        range(14), (400, 400, 880, 200, 200, 400, 880, 200, 200, 200, 200, 1080, 400, 1)
    ):
        if iteration in (3, 5, 6, 8, 10):  # creating convert requests
            transaction = alice.convert_hives(tt.Asset.Test(convert_amount), broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=transaction)
            convert_amount += 5
            convert_transactions.append(transaction)
            alice.assert_collateralized_conversion_requests(trx=transaction, state="create")
            alice.assert_account_balance_after_creating_collateral_conversion_request(transaction)
            node.api.debug_node.debug_generate_blocks(
                debug_key=tt.Account("initminer").private_key,
                count=2,
                skip=0,
                miss_blocks=0,
            )
            alice.assert_collateralized_convert_immediate_conversion_operation(
                expected_amount=len(convert_transactions)
            )
        if iteration in (0, 1, 2, 4, 5, 10):  # setting up feed prices
            trx = publish_feeds(node, wallet, next(feeds_it), 1000, broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=trx)
        if iteration in (7, 9, 11, 12, 13):  # checking if convert requests were completed
            vops_counter += 1
            alice.assert_if_right_amount_of_hives_came_back_after_collateral_conversion(
                convert_transactions[convert_transactions_it]
            )

            alice.assert_collateralized_conversion_requests(
                trx=convert_transactions[convert_transactions_it], state="delete"
            )
            alice.assert_fill_collateralized_convert_request_operation(expected_amount=vops_counter)
            convert_transactions_it += 1

        generate_large_amount_of_blocks(node=node, block_signer=tt.Account("initminer"), count=blocks_to_generate)
