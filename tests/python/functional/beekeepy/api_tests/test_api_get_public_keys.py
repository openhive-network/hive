from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo
    from clive_local_tools.data.types import WalletsGeneratorT


async def open_and_unlock_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    await beekeeper.api.open(wallet_name=wallet.name)
    await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)


@pytest.mark.parametrize("explicit_wallet_name", [False, True])
async def test_api_get_public_keys(
    beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT, explicit_wallet_name: bool
) -> None:
    """Test test_api_get_public_keys will test beekeeper_api.get_public_keys api call."""
    # ARRANGE
    wallets = await setup_wallets(1, import_keys=False, keys_per_wallet=5)
    wallet = wallets[0]

    explicit_wallet_name_param = {"wallet_name": wallet.name} if explicit_wallet_name else {}

    for pair in wallet.keys.pairs:
        await beekeeper.api.import_key(wallet_name=wallet.name, wif_key=pair.private_key.value)

    # ACT
    response = (await beekeeper.api.get_public_keys(**explicit_wallet_name_param)).keys
    bk_public_keys = [pub.public_key for pub in response]

    # ASSERT
    assert bk_public_keys == wallet.keys.get_public_keys(), "All keys should be available."


async def test_api_get_public_keys_with_different_wallet_name(
    beekeeper: Beekeeper, wallet: WalletInfo  # noqa: ARG001
) -> None:
    # ARRANGE
    not_existing_wallet_name = "not-existing"

    # ACT & ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet {not_existing_wallet_name} is locked"):
        await beekeeper.api.get_public_keys(wallet_name=not_existing_wallet_name)


async def test_api_get_public_keys_with_many_wallets(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_get_public_keys_with_many_wallets will test beekeeper_api.get_public_keys when wallets are being locked."""
    # ARRANGE
    # Prepare wallets
    wallets = await setup_wallets(2, import_keys=True, keys_per_wallet=5)
    wallet_1 = wallets[0]
    wallet_2 = wallets[1]
    all_keys = wallet_1.keys.get_public_keys() + wallet_2.keys.get_public_keys()
    all_keys.sort()
    # open and and unlock wallets
    await open_and_unlock_wallet(wallet=wallet_1, beekeeper=beekeeper)
    await open_and_unlock_wallet(wallet=wallet_2, beekeeper=beekeeper)

    # Get ALL public key from bk it should contain both, wallet_1_keys and  wallet_2_keys
    bk_pub_keys_all = [pub.public_key for pub in (await beekeeper.api.get_public_keys()).keys]
    bk_pub_keys_all.sort()
    assert bk_pub_keys_all == all_keys, "All keys should be available."

    # ACT & ASSERT 1
    # Lock wallet 2
    await beekeeper.api.lock(wallet_name=wallet_2.name)
    # Now only keys from wallet 1 should be available
    bk_pub_keys_1 = [pub.public_key for pub in (await beekeeper.api.get_public_keys()).keys]
    assert bk_pub_keys_1 == wallet_1.keys.get_public_keys(), "Only keys from wallet 1 should be available."

    # ACT & ASSERT 2
    # Lock wallet 1
    await beekeeper.api.lock(wallet_name=wallet_1.name)
    # Now all wallet are closed, so that no key should be available
    with pytest.raises(CommunicationError, match="You don't have any unlocked wallet"):
        await beekeeper.api.get_public_keys()


async def test_api_get_public_keys_with_many_wallets_closed(
    beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT
) -> None:
    """Test test_api_get_public_keys_with_many_wallets_closed will test beekeeper_api.get_public_keys when wallets are being closed."""
    # ARRANGE
    # Prepare wallets
    wallets = await setup_wallets(2, import_keys=True, keys_per_wallet=5)
    wallet_1 = wallets[0]
    wallet_2 = wallets[1]
    all_keys = wallet_1.keys.get_public_keys() + wallet_2.keys.get_public_keys()
    all_keys.sort()

    # Open and unlock wallets
    await open_and_unlock_wallet(wallet=wallet_1, beekeeper=beekeeper)
    await open_and_unlock_wallet(wallet=wallet_2, beekeeper=beekeeper)

    # Get all available public keys ()
    bk_pub_keys_all = [pub.public_key for pub in (await beekeeper.api.get_public_keys()).keys]
    bk_pub_keys_all.sort()
    assert bk_pub_keys_all == all_keys, "Keys from wallet 1 and wallet 2 should be available."

    # ACT & ASSERT 1
    # Close wallet 2
    await beekeeper.api.close(wallet_name=wallet_2.name)
    bk_pub_keys_1 = [pub.public_key for pub in (await beekeeper.api.get_public_keys()).keys]
    assert bk_pub_keys_1 == wallet_1.keys.get_public_keys(), "Only keys from wallet 1 should be available."

    # ACT & ASSERT 2
    # Close wallet 1,
    await beekeeper.api.close(wallet_name=wallet_1.name)
    # There is no wallet
    with pytest.raises(CommunicationError, match="You don't have any wallet"):
        await beekeeper.api.get_public_keys()
