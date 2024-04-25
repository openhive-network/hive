from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError
from clive_local_tools.data.models import Keys, WalletInfo

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.types import WalletsGeneratorT


async def test_api_remove_key(
    beekeeper: Beekeeper,
    wallet_no_keys: WalletInfo,
) -> None:
    """Test test_api_remove_key will test beekeeper_api_remove_key.."""
    # ARRANGE
    for pair in Keys(5).pairs:
        await beekeeper.api.import_key(wallet_name=wallet_no_keys.name, wif_key=pair.private_key.value)

        # ACT
        await beekeeper.api.remove_key(
            wallet_name=wallet_no_keys.name, password=wallet_no_keys.password, public_key=pair.public_key.value
        )

    # ASSERT
    assert len((await beekeeper.api.get_public_keys()).keys) == 0, "There should be no keys left."


async def test_api_remove_key_from_locked(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_remove_key_from_locked will try to remove key from locker wallet."""
    # ARRANGE & ACT
    await beekeeper.api.lock(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet is locked: {wallet.name}"):
        await beekeeper.api.remove_key(
            wallet_name=wallet.name, password=wallet.password, public_key=wallet.keys.pairs[0].public_key.value
        )


async def test_api_remove_key_from_closed(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_remove_key_from_closed will try to remove key from closed wallet."""
    # ARRANGE & ACT
    await beekeeper.api.close(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet not found: {wallet.name}"):
        await beekeeper.api.remove_key(
            wallet_name=wallet.name, password=wallet.password, public_key=wallet.keys.pairs[0].public_key.value
        )


async def test_api_remove_key_simple_scenario(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_remove_key_simple_scenario will test simple flow of importing and removing keys."""
    # ARRANGE
    wallets = await setup_wallets(1, import_keys=True, keys_per_wallet=5)
    wallet = wallets[0]
    key_to_remove = wallet.keys.pairs.pop(3)

    # Get keys before removing
    bk_keys_before = (await beekeeper.api.get_public_keys()).keys
    bk_pub_keys_before = [pub_key.public_key for pub_key in bk_keys_before]

    # Check if key exist
    assert key_to_remove.public_key.value in bk_pub_keys_before, "Check if target key exists."

    # ACT
    await beekeeper.api.remove_key(
        wallet_name=wallet.name, password=wallet.password, public_key=key_to_remove.public_key.value
    )

    # ASSERT
    bk_keys_after = (await beekeeper.api.get_public_keys()).keys
    bk_pub_keys_after = [pub_key.public_key for pub_key in bk_keys_after]

    # Check if key was removed
    assert key_to_remove.public_key not in bk_keys_after, "Recently removed key shouldn't not be listed."
    # Check if other keys still exists
    bk_pub_keys_before_copy = bk_pub_keys_before.copy()
    bk_pub_keys_before_copy.remove(key_to_remove.public_key.value)
    assert bk_pub_keys_before_copy == bk_pub_keys_after, "Check if beekeeper removes only target key."
