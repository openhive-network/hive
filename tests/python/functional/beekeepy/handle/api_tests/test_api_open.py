from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.beekeeper.models import WalletInfo

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper


def test_api_open(beekeeper: Beekeeper) -> None:
    """Test test_api_open will test beekeeper_api.open api call."""
    # ARRANGE
    wallet = WalletInfo(name="test_api_open", password="test_api_open")
    wallet_path = beekeeper.settings.working_directory / f"{wallet.name}.wallet"
    assert wallet_path.exists() is False, "Before creation there should be no wallet file."
    beekeeper.api.beekeeper.create(wallet_name=wallet.name, password=wallet.password)

    # ACT
    beekeeper.api.beekeeper.open(wallet_name=wallet.name)

    # ASSERT
    assert wallet_path.exists() is True, "After creation there should be wallet file."
    assert (
        wallet.name == (beekeeper.api.beekeeper.list_wallets()).wallets[0].name
    ), "After creation wallet should be visible in beekeeper."


def test_api_reopen_already_opened(beekeeper: Beekeeper) -> None:
    """Test test_api_reopen_already_opened will try to open already opened wallet."""
    # ARRANGE
    wallet = WalletInfo(name="test_api_open", password="test_api_open")
    beekeeper.api.beekeeper.create(wallet_name=wallet.name, password=wallet.password)

    # ACT
    beekeeper.api.beekeeper.open(wallet_name=wallet.name)
    assert len((beekeeper.api.beekeeper.list_wallets()).wallets) == 1, "There should be 1 wallet opened."
    beekeeper.api.beekeeper.open(wallet_name=wallet.name)

    # ASSERT
    assert len((beekeeper.api.beekeeper.list_wallets()).wallets) == 1, "There should be 1 wallet opened."


def test_api_reopen_closed(beekeeper: Beekeeper) -> None:
    """Test test_api_reopen_closed will try to open closed wallet."""
    # ARRANGE
    wallet = WalletInfo(name="test_api_open", password="test_api_open")
    beekeeper.api.beekeeper.create(wallet_name=wallet.name, password=wallet.password)

    # ACT
    beekeeper.api.beekeeper.open(wallet_name=wallet.name)
    assert len((beekeeper.api.beekeeper.list_wallets()).wallets) == 1, "There should be 1 wallet opened."

    beekeeper.api.beekeeper.close(wallet_name=wallet.name)
    assert len((beekeeper.api.beekeeper.list_wallets()).wallets) == 0, "There should be no wallet opened."

    beekeeper.api.beekeeper.open(wallet_name=wallet.name)

    # ASSERT
    assert len((beekeeper.api.beekeeper.list_wallets()).wallets) == 1, "There should be no wallet opened."
