"""
Tests for the fee/balance validation of ``account_create_operation``.

The high-level ``wallet.api.create_account`` always fills ``fee`` with the chain's
current ``account_creation_fee``, so it can never exercise the "wrong fee" path of the
``account_create_evaluator``. These tests build a raw ``account_create_operation`` with an
explicit fee to cover:
  * the fee assertion (``HIVE_CHAIN_FEE_ASSERT`` -> ``chain/fee``) — wrong fee paid, and
  * the insufficient-balance path — a valid fee the creator cannot afford.

Related to https://gitlab.syncad.com/hive/hive/-/issues/856
"""

from __future__ import annotations

import pytest
from test_tools.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.functional.python import basic_authority
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation
from schemas.operations.account_create_operation import AccountCreateOperation

# Value configured by the `prepared_node` fixture (see operation_tests/conftest.py).
ACCOUNT_CREATION_FEE = tt.Asset.Test(3)


def _account_create_operation(creator: str, new_account_name: str, fee: tt.Asset.TestT) -> AccountCreateOperation:
    key = tt.Account(new_account_name).public_key
    return AccountCreateOperation(
        creator=creator,
        new_account_name=new_account_name,
        fee=fee,
        owner=basic_authority(key),
        active=basic_authority(key),
        posting=basic_authority(key),
        memo_key=key,
        json_metadata="{}",
    )


def _account_exists(node: tt.InitNode, name: str) -> bool:
    return len(node.api.database.find_accounts(accounts=[name]).accounts) == 1


@pytest.mark.testnet()
def test_account_create_with_exact_fee(prepared_node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Paying exactly the account creation fee creates the account (control case)."""
    assert prepared_node.api.wallet_bridge.get_chain_properties().account_creation_fee == ACCOUNT_CREATION_FEE

    create_transaction_with_any_operation(
        wallet, [_account_create_operation("initminer", "alice", ACCOUNT_CREATION_FEE)]
    )

    assert _account_exists(prepared_node, "alice"), "Account was not created despite paying the exact fee."


@pytest.mark.testnet()
def test_account_create_with_excessive_fee(prepared_node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Paying more than the account creation fee is rejected by the fee assertion."""
    assert prepared_node.api.wallet_bridge.get_chain_properties().account_creation_fee == ACCOUNT_CREATION_FEE

    with pytest.raises(ErrorInResponseError) as exception:
        create_transaction_with_any_operation(
            wallet, [_account_create_operation("initminer", "alice", ACCOUNT_CREATION_FEE + tt.Asset.Test(1))]
        )

    assert "Must pay the exact account creation fee." in str(exception.value)
    assert not _account_exists(prepared_node, "alice"), "Account was created despite paying the wrong fee."


@pytest.mark.testnet()
def test_account_create_with_insufficient_fee(prepared_node: tt.InitNode, wallet: tt.Wallet) -> None:
    """Paying less than the account creation fee is rejected by the fee assertion."""
    assert prepared_node.api.wallet_bridge.get_chain_properties().account_creation_fee == ACCOUNT_CREATION_FEE

    with pytest.raises(ErrorInResponseError) as exception:
        create_transaction_with_any_operation(
            wallet, [_account_create_operation("initminer", "alice", ACCOUNT_CREATION_FEE - tt.Asset.Test(1))]
        )

    assert "Must pay the exact account creation fee." in str(exception.value)
    assert not _account_exists(prepared_node, "alice"), "Account was created despite paying the wrong fee."


@pytest.mark.testnet()
def test_account_create_with_insufficient_balance(prepared_node: tt.InitNode, wallet: tt.Wallet) -> None:
    """A creator paying the valid fee but lacking the HIVE balance to cover it is rejected."""
    assert prepared_node.api.wallet_bridge.get_chain_properties().account_creation_fee == ACCOUNT_CREATION_FEE

    # `poor` has resource credits (vests) to broadcast, but no HIVE to pay the (valid) creation fee.
    wallet.create_account("poor", hives=0, vests=50)

    with pytest.raises(ErrorInResponseError) as exception:
        create_transaction_with_any_operation(wallet, [_account_create_operation("poor", "bob", ACCOUNT_CREATION_FEE)])

    assert "does not have sufficient funds for balance adjustment" in str(exception.value)
    assert not _account_exists(prepared_node, "bob"), "Account was created despite the creator's insufficient balance."
