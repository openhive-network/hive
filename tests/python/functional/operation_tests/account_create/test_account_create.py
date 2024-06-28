"""
All test cases are described in https://gitlab.syncad.com/hive/hive/-/issues/695
"""
from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account, assert_account_was_created
from hive_local_tools.functional.python.operation.account_create import AccountCreate


@pytest.mark.parametrize("account_creation_fee", [tt.Asset.Test(100)], indirect=["account_creation_fee"])
@pytest.mark.testnet()
def test_create_account_operation(
    create_node_and_wallet: tuple, alice: Account, account_creation_fee: tt.Asset.TestT
) -> None:
    node, wallet = create_node_and_wallet

    balance_before_account_creation = alice.get_hive_balance()

    account_creation = AccountCreate(
        node,
        wallet=wallet,
        creator=alice.name,
        fee=account_creation_fee,
        new_account_name="bob",
    )
    balance_after_account_creation = alice.get_hive_balance()

    alice.check_if_current_rc_mana_was_reduced(account_creation.trx)
    assert balance_after_account_creation + account_creation_fee == balance_before_account_creation
    assert wallet.api.get_account("bob")
    assert_account_was_created(node, "bob")


@pytest.mark.parametrize("account_creation_fee", [tt.Asset.Test(100)], indirect=["account_creation_fee"])
@pytest.mark.testnet()
def test_try_to_create_account_operation_without_enough_hives(
    create_node_and_wallet: tuple, alice_without_hive: Account, account_creation_fee: tt.Asset.TestT
) -> None:
    node, wallet = create_node_and_wallet

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        AccountCreate(
            node,
            wallet=wallet,
            creator=alice_without_hive.name,
            fee=account_creation_fee,
            new_account_name="bob",
        )

    assert (
        f"Account {alice_without_hive.name} does not have sufficient funds for balance adjustment."
        in exception.value.error
    )


@pytest.mark.parametrize("account_creation_fee", [tt.Asset.Test(100)], indirect=["account_creation_fee"])
@pytest.mark.testnet()
def test_create_account_operation_for_already_existing_account(
    create_node_and_wallet: tuple, alice: Account, account_creation_fee: tt.Asset.TestT
) -> None:
    node, wallet = create_node_and_wallet
    wallet.api.create_account(alice.name, "bob", "{}")

    balance_before_account_creation = alice.get_hive_balance()
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        AccountCreate(
            node,
            wallet=wallet,
            creator=alice.name,
            fee=account_creation_fee,
            new_account_name="bob",
        )
    balance_after_account_creation = alice.get_hive_balance()

    assert (
        "could not insert object, most likely a uniqueness constraint was violated inside index holding types"
        in exception.value.error
    )
    assert balance_after_account_creation == balance_before_account_creation


@pytest.mark.parametrize("account_creation_fee", [tt.Asset.Test(100)], indirect=["account_creation_fee"])
@pytest.mark.testnet()
def test_create_account_operation_with_too_much_fee(
    create_node_and_wallet: tuple, alice: Account, account_creation_fee: tt.Asset.TestT
) -> None:
    node, wallet = create_node_and_wallet

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        AccountCreate(
            node,
            wallet=wallet,
            creator=alice.name,
            fee=account_creation_fee + tt.Asset.Test(10),
            new_account_name="bob",
        )

    assert (
        "Exception:o.fee == wso.median_props.account_creation_fee: Must pay the exact account creation fee."
        in exception.value.error
    )


@pytest.mark.parametrize("account_creation_fee", [tt.Asset.Test(100)], indirect=["account_creation_fee"])
@pytest.mark.testnet()
def test_create_account_operation_with_too_less_fee(
    create_node_and_wallet: tuple, alice: Account, account_creation_fee: tt.Asset.TestT
) -> None:
    node, wallet = create_node_and_wallet

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        AccountCreate(
            node,
            wallet=wallet,
            creator=alice.name,
            fee=account_creation_fee - tt.Asset.Test(10),
            new_account_name="bob",
        )

    assert (
        "Exception:o.fee == wso.median_props.account_creation_fee: Must pay the exact account creation fee."
        in exception.value.error
    )
