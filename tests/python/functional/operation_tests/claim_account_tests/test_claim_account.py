"""https://gitlab.syncad.com/hive/hive/-/issues/693"""
from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation.claim_account import ClaimAccountToken

if TYPE_CHECKING:
    from hive_local_tools.functional.python.operation import Account


@pytest.mark.testnet()
@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_crate_claim_account_token_by_hive(
    node: tt.InitNode, wallet_alice: tt.Wallet, alice: Account, fee: tt.Asset.TestT
) -> None:
    """User has enough Hive to claim a new account token."""
    alice.top_up(tt.Asset.Test(3))

    create_token = ClaimAccountToken(node, wallet_alice, alice.name, fee)

    assert alice.get_pending_claimed_accounts() == 1, "Claim token was not created."
    create_token.assert_minimal_operation_rc_cost()
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=create_token.rc_cost,
        operation_timestamp=create_token.timestamp,
    )
    assert alice.get_hive_balance() == alice.hive - fee, "The token creation fee was not charged."
    assert alice.get_hive_balance() == tt.Asset.Test(0)


@pytest.mark.testnet()
@pytest.mark.parametrize("fee", [tt.Asset.Test(0)], indirect=["fee"])
def test_crate_claim_account_token_by_rc(
    node: tt.InitNode, initminer: Account, initminer_wallet: tt.Wallet, fee: tt.Asset.TestT
) -> None:
    """User has enough RC to claim a new account token."""
    create_token = ClaimAccountToken(node, initminer_wallet, initminer.name, tt.Asset.Test(0))

    assert initminer.get_pending_claimed_accounts() == 1, "Claim token was not created."
    create_token.assert_minimal_operation_rc_cost(minimal_cost=1_000_000_000_000)
    assert initminer.get_hive_balance() == initminer.hive - fee, "The token creation fee was not charged."


@pytest.mark.testnet()
@pytest.mark.parametrize("fee", [tt.Asset.Test(0)], indirect=["fee"])
def test_crate_claim_account_token_without_enough_rc(
    node: tt.InitNode, alice: Account, wallet_alice: tt.Wallet, fee: tt.Asset.TestT
) -> None:
    """User doesn't have enough RC to crate a token"""
    assert alice.vest == tt.Asset.Vest(0)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.claim_account_creation(alice.name, fee)

    assert "Account: alice has 0 RC, needs " in exception.value.error
    assert (
        alice.get_pending_claimed_accounts() == 0
    ), "Claim token was created. It should not be possible to create a token without enough RC."


@pytest.mark.testnet()
@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_crate_claim_account_token_without_enough_hive_balance_to_cover_fee(
    node: tt.InitNode, alice: Account, wallet_alice: tt.Wallet, fee: tt.Asset.TestT
) -> None:
    """A user tries to claim a token in HIVE, but he doesn't have enough HIVE."""
    assert alice.hive == tt.Asset.Test(0)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.claim_account_creation(alice.name, fee)

    assert "Insufficient balance to create account." in exception.value.error
    assert (
        alice.get_pending_claimed_accounts() == 0
    ), "Claim token was created. It should not be possible to create a token without enough RC."


@pytest.mark.testnet()
@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_crate_claim_account_token_with_insufficient_fee(
    node: tt.InitNode, alice: Account, wallet_alice: tt.Wallet, fee: tt.Asset.TestT
) -> None:
    """A user tries to claim a token in HIVE, but he gives incorrect fee amount - too much."""
    alice.top_up(tt.Asset.Test(4))
    assert alice.get_hive_balance() > fee

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.claim_account_creation(alice.name, fee + tt.Asset.Test(1))

    assert "Cannot pay more than account creation fee." in exception.value.error
    assert (
        alice.get_pending_claimed_accounts() == 0
    ), "Claim token was created. It should not be possible to create a token without enough RC."


@pytest.mark.testnet()
@pytest.mark.parametrize("fee", [tt.Asset.Test(3)], indirect=["fee"])
def test_claim_account_creation_token_with_excessive_fee(
    node: tt.InitNode, alice: Account, wallet_alice: tt.Wallet, fee: tt.Asset.TestT
) -> None:
    """A user tries to claim a token in HIVE, but he gives incorrect fee amount - too much."""
    alice.top_up(tt.Asset.Test(4))
    assert alice.get_hive_balance() > fee

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.claim_account_creation(alice.name, fee - tt.Asset.Test(1))

    # fixme: change assert after repair: https://gitlab.syncad.com/hive/hive/-/issues/696
    assert "Cannot pay more than account creation fee." in exception.value.error
    assert (
        alice.get_pending_claimed_accounts() == 0
    ), "Claim token was created. It should not be possible to create a token without enough RC."
