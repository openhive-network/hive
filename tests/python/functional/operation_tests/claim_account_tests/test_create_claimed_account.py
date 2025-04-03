"""https://gitlab.syncad.com/hive/hive/-/issues/694"""
from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.functional.python.operation import assert_account_was_created
from hive_local_tools.functional.python.operation.claim_account import (
    ClaimAccountToken,
    CreateClaimedAccount,
)

if TYPE_CHECKING:
    from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def create_new_account_token_for_alice(node: tt.InitNode, wallet_alice: tt.Wallet, alice: Account) -> ClaimAccountToken:
    """User has enough Hive to claim a new account token."""
    alice.top_up(tt.Asset.Test(3))

    create_token = ClaimAccountToken(node, wallet_alice, alice.name, tt.Asset.Test(3))

    assert alice.get_pending_claimed_accounts() == 1, "Claim token was not created."

    return create_token


@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_create_claimed_account_operation(
    node: tt.InitNode,
    initminer_wallet: tt.Wallet,
    wallet_alice: tt.Wallet,
    alice: Account,
    create_new_account_token_for_alice: ClaimAccountToken,
    fee: tt.Asset.TestT,
) -> None:
    initminer_wallet.api.delegate_rc("initminer", [alice.name], 200000000000)

    alice.update_account_info()

    create_claimed_account = CreateClaimedAccount(node, wallet_alice, alice.name, "bob")

    assert alice.get_pending_claimed_accounts() == 0, "Claim token was not used."
    alice.check_if_current_rc_mana_was_reduced(create_claimed_account.trx)
    assert wallet_alice.api.get_account("bob"), "Account named bob does not exist"
    assert_account_was_created(node, "bob")


@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_try_to_create_claimed_account_operation_without_available_token(
    node: tt.InitNode, wallet_alice: tt.Wallet, alice: Account, fee: tt.Asset.TestT
) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        CreateClaimedAccount(node, wallet_alice, alice.name, "bob")

    assert "alice has no claimed accounts to create" in exception.value.error


@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_try_to_create_claimed_account_operation_with_already_existing_account(
    node: tt.InitNode,
    initminer_wallet: tt.Wallet,
    wallet_alice: tt.Wallet,
    alice: Account,
    create_new_account_token_for_alice: ClaimAccountToken,
    fee: tt.Asset.TestT,
) -> None:
    initminer_wallet.create_account("bob")
    initminer_wallet.api.delegate_rc("initminer", [alice.name], 200000000000)

    with pytest.raises(ErrorInResponseError) as exception:
        CreateClaimedAccount(node, wallet_alice, alice.name, "bob")

    assert "Account bob already exists." in exception.value.error
    assert alice.get_pending_claimed_accounts() == 1, "Claim token was used. Shouldn't be."
