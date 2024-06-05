from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools.beekeeper.models import WalletInfo

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper


def test_api_list_wallets(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_list_wallets will test beekeeper_api.list_wallets api call."""
    # ARRANGE & ACT & ASSERT
    assert wallet.name == (beekeeper.api.list_wallets()).wallets[0].name, "There should be only one wallet"


def test_api_list_wallets_dynamic_number(beekeeper: Beekeeper) -> None:
    """Test test_api_list_wallets_dynamic_number will test beekeeper_api.list_wallets checks if follows number of open/closed wallets."""
    # ARRANGE
    wallets = [
        WalletInfo(name="name1", password="pas1"),
        WalletInfo(name="name2", password="pas2"),
        WalletInfo(name="name3", password="pas3"),
    ]

    # ACT & ASSERT
    for number, wallet in enumerate(wallets):
        beekeeper.api.create(wallet_name=wallet.name, password=wallet.password)
        assert number + 1 == len(
            (beekeeper.api.list_wallets()).wallets
        ), "Number of wallets should match the ones kept by beekeeper"

    for number, wallet in reversed(list(enumerate(wallets))):
        beekeeper.api.close(wallet_name=wallet.name)
        assert number == len(
            (beekeeper.api.list_wallets()).wallets
        ), "Number of wallets should match the ones kept by beekeeper"
