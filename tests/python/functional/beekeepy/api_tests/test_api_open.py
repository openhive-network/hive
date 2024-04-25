from __future__ import annotations

from typing import TYPE_CHECKING

from clive_local_tools.data.models import WalletInfo

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper


async def test_api_open(beekeeper: Beekeeper) -> None:
    """Test test_api_open will test beekeeper_api.open api call."""
    # ARRANGE
    wallet = WalletInfo(name="test_api_open", password="test_api_open")
    wallet_path = beekeeper.get_wallet_dir() / f"{wallet.name}.wallet"
    assert wallet_path.exists() is False, "Before creation there should be no wallet file."
    await beekeeper.api.create(wallet_name=wallet.name, password=wallet.password)

    # ACT
    await beekeeper.api.open(wallet_name=wallet.name)

    # ASSERT
    assert wallet_path.exists() is True, "After creation there should be wallet file."
    assert (
        wallet.name == (await beekeeper.api.list_wallets()).wallets[0].name
    ), "After creation wallet should be visible in beekeeper."


async def test_api_reopen_already_opened(beekeeper: Beekeeper) -> None:
    """Test test_api_reopen_already_opened will try to open already opened wallet."""
    # ARRANGE
    wallet = WalletInfo(name="test_api_open", password="test_api_open")
    await beekeeper.api.create(wallet_name=wallet.name, password=wallet.password)

    # ACT
    await beekeeper.api.open(wallet_name=wallet.name)
    assert len((await beekeeper.api.list_wallets()).wallets) == 1, "There should be 1 wallet opened."
    await beekeeper.api.open(wallet_name=wallet.name)

    # ASSERT
    assert len((await beekeeper.api.list_wallets()).wallets) == 1, "There should be 1 wallet opened."


async def test_api_reopen_closed(beekeeper: Beekeeper) -> None:
    """Test test_api_reopen_closed will try to open closed wallet."""
    # ARRANGE
    wallet = WalletInfo(name="test_api_open", password="test_api_open")
    await beekeeper.api.create(wallet_name=wallet.name, password=wallet.password)

    # ACT
    await beekeeper.api.open(wallet_name=wallet.name)
    assert len((await beekeeper.api.list_wallets()).wallets) == 1, "There should be 1 wallet opened."

    await beekeeper.api.close(wallet_name=wallet.name)
    assert len((await beekeeper.api.list_wallets()).wallets) == 0, "There should be no wallet opened."

    await beekeeper.api.open(wallet_name=wallet.name)

    # ASSERT
    assert len((await beekeeper.api.list_wallets()).wallets) == 1, "There should be no wallet opened."
