from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    import test_tools as tt
    from hive_local_tools.beekeeper.models import WalletInfo, WalletsGeneratorT


def test_api_import_key(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_import_key will test beekeeper_api.import_key api call."""
    # ARRANGE
    wallets = setup_wallets(1, import_keys=False, keys_per_wallet=5)
    wallet = wallets[0]

    # ACT & ASSERT
    for acc in wallet.accounts:
        assert (
            acc.public_key == (beekeeper.api.import_key(wallet_name=wallet.name, wif_key=acc.private_key)).public_key
        ), "Public key of imported wif should match."


def test_api_import_key_to_locked(beekeeper: Beekeeper, wallet: WalletInfo, keys_to_import: list[tt.Account]) -> None:
    """Test test_api_import_key_to_locked will test beekeeper_api.import_key to the locked wallet."""
    # ARRANGE & ACT
    beekeeper.api.lock(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(RequestError, match=f"Wallet is locked: {wallet.name}"):
        beekeeper.api.import_key(wallet_name=wallet.name, wif_key=keys_to_import[0].private_key)


def test_api_import_key_to_closed(beekeeper: Beekeeper, wallet: WalletInfo, keys_to_import: list[tt.Account]) -> None:
    """Test test_api_import_key_to_closed will test beekeeper_api.import_key to the closed wallet."""
    # ARRANGE & ACT
    beekeeper.api.close(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(RequestError, match=f"Wallet not found: {wallet.name}"):
        beekeeper.api.import_key(wallet_name=wallet.name, wif_key=keys_to_import[0].private_key)
