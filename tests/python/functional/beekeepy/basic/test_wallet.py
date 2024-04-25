from __future__ import annotations

import asyncio
import json
from typing import TYPE_CHECKING, Final

import pytest

from clive.exceptions import CommunicationError

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive.__private.core.beekeeper.model import ListWallets
    from clive_local_tools.data.models import WalletInfo


def check_wallets(given: ListWallets, valid: list[str], *, unlocked: bool = True) -> None:
    assert len(given.wallets) == len(valid)
    for given_wallet in given.wallets:
        assert given_wallet.name in valid
        assert given_wallet.unlocked == unlocked


@pytest.mark.parametrize("wallet_name", ["test", "123"])
async def test_create_wallet(beekeeper: Beekeeper, wallet_name: str) -> None:
    # ARRANGE & ACT
    await beekeeper.api.create(wallet_name=wallet_name)

    # ASSERT
    check_wallets(await beekeeper.api.list_wallets(), [wallet_name])


@pytest.mark.parametrize("invalid_wallet_name", [(",,,", "*", "   a   ", " ", "", json.dumps({"a": None, "b": 21.37}))])
async def test_invalid_wallet_names(beekeeper: Beekeeper, invalid_wallet_name: str) -> None:
    # ARRANGE, ACT & ASSERT
    with pytest.raises(CommunicationError):
        await beekeeper.api.create(wallet_name=invalid_wallet_name)


async def test_wallet_open(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    await beekeeper.restart()  # this will close

    # ACT & ASSERT
    check_wallets(await beekeeper.api.list_wallets(), [])
    await beekeeper.api.open(wallet_name=wallet.name)
    check_wallets(await beekeeper.api.list_wallets(), [wallet.name], unlocked=False)


async def test_wallet_unlock(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    await beekeeper.api.lock_all()  # after creation wallet is opened and unlocked by default

    # ACT
    await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)

    # ASSERT
    check_wallets(await beekeeper.api.list_wallets(), [wallet.name])


async def test_timeout(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    timeout: Final[int] = 5
    comparison_error_max_delta: Final[float] = 1.0

    # ARRANGE
    await beekeeper.api.set_timeout(seconds=timeout)

    # ASSERT
    info = await beekeeper.api.get_info()
    assert timeout - (info.timeout_time - info.now).total_seconds() <= comparison_error_max_delta
    check_wallets(await beekeeper.api.list_wallets(), [wallet.name])

    # ACT
    await asyncio.sleep(timeout)

    # ASSERT
    check_wallets(await beekeeper.api.list_wallets(), [wallet.name], unlocked=False)


async def test_create_wallet_with_custom_password(beekeeper: Beekeeper, wallet_name: str) -> None:
    # ARRANGE & ACT
    password = (await beekeeper.api.create(wallet_name=wallet_name, password=wallet_name)).password

    # ASSERT
    assert password == wallet_name
