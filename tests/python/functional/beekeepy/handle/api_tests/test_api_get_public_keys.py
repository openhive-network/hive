from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo, WalletsGeneratorT


def open_and_unlock_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    beekeeper.api.open(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)


@pytest.mark.parametrize("explicit_wallet_name", [False, True])
def test_api_get_public_keys(
    beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT, explicit_wallet_name: bool
) -> None:
    """Test test_api_get_public_keys will test beekeeper_api.get_public_keys api call."""
    # ARRANGE
    wallets = setup_wallets(1, import_keys=False, keys_per_wallet=5)
    wallet = wallets[0]

    explicit_wallet_name_param = {"wallet_name": wallet.name} if explicit_wallet_name else {}

    for account in wallet.accounts:
        beekeeper.api.import_key(wallet_name=wallet.name, wif_key=account.private_key)

    # ACT
    response = (beekeeper.api.get_public_keys(**explicit_wallet_name_param)).keys
    bk_public_keys = [pub.public_key for pub in response]

    # ASSERT
    assert sorted(bk_public_keys) == sorted(wallet.get_all_public_keys()), "All keys should be available."


def test_api_get_public_keys_with_different_wallet_name(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    not_existing_wallet_name = "not-existing"

    # ACT & ASSERT
    with pytest.raises(RequestError, match=f"Wallet {not_existing_wallet_name} is locked"):
        beekeeper.api.get_public_keys(wallet_name=not_existing_wallet_name)


def test_api_get_public_keys_with_many_wallets(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_get_public_keys_with_many_wallets will test beekeeper_api.get_public_keys when wallets are being locked."""
    # ARRANGE
    # Prepare wallets
    wallets = setup_wallets(2, import_keys=True, keys_per_wallet=5)
    wallet_1 = wallets[0]
    wallet_2 = wallets[1]
    all_keys = wallet_1.get_all_public_keys() + wallet_2.get_all_public_keys()
    all_keys.sort()
    # open and and unlock wallets
    open_and_unlock_wallet(wallet=wallet_1, beekeeper=beekeeper)
    open_and_unlock_wallet(wallet=wallet_2, beekeeper=beekeeper)

    # Get ALL public key from bk it should contain both, wallet_1_keys and  wallet_2_keys
    bk_pub_keys_all = [pub.public_key for pub in (beekeeper.api.get_public_keys()).keys]
    bk_pub_keys_all.sort()
    assert bk_pub_keys_all == all_keys, "All keys should be available."

    # ACT & ASSERT 1
    # Lock wallet 2
    beekeeper.api.lock(wallet_name=wallet_2.name)
    # Now only keys from wallet 1 should be available
    bk_pub_keys_1 = [pub.public_key for pub in (beekeeper.api.get_public_keys()).keys]
    assert sorted(bk_pub_keys_1) == sorted(
        wallet_1.get_all_public_keys()
    ), "Only keys from wallet 1 should be available."

    # ACT & ASSERT 2
    # Lock wallet 1
    beekeeper.api.lock(wallet_name=wallet_1.name)
    # Now all wallet are closed, so that no key should be available
    with pytest.raises(RequestError, match="You don't have any unlocked wallet"):
        beekeeper.api.get_public_keys()


def test_api_get_public_keys_with_many_wallets_closed(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_get_public_keys_with_many_wallets_closed will test beekeeper_api.get_public_keys when wallets are being closed."""
    # ARRANGE
    # Prepare wallets
    wallets = setup_wallets(2, import_keys=True, keys_per_wallet=5)
    wallet_1 = wallets[0]
    wallet_2 = wallets[1]
    all_keys = wallet_1.get_all_public_keys() + wallet_2.get_all_public_keys()
    all_keys.sort()

    # Open and unlock wallets
    open_and_unlock_wallet(wallet=wallet_1, beekeeper=beekeeper)
    open_and_unlock_wallet(wallet=wallet_2, beekeeper=beekeeper)

    # Get all available public keys ()
    bk_pub_keys_all = [pub.public_key for pub in (beekeeper.api.get_public_keys()).keys]
    bk_pub_keys_all.sort()
    assert bk_pub_keys_all == all_keys, "Keys from wallet 1 and wallet 2 should be available."

    # ACT & ASSERT 1
    # Close wallet 2
    beekeeper.api.close(wallet_name=wallet_2.name)
    bk_pub_keys_1 = [pub.public_key for pub in (beekeeper.api.get_public_keys()).keys]
    assert sorted(bk_pub_keys_1) == sorted(
        wallet_1.get_all_public_keys()
    ), "Only keys from wallet 1 should be available."

    # ACT & ASSERT 2
    # Close wallet 1,
    beekeeper.api.close(wallet_name=wallet_1.name)
    # There is no wallet
    with pytest.raises(RequestError, match="You don't have any wallet"):
        beekeeper.api.get_public_keys()
