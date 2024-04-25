from __future__ import annotations

import json
import time
from typing import TYPE_CHECKING, Final

import pytest
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo
    from schemas.apis.beekeeper_api import ListWallets


def check_wallets(given: ListWallets, valid: list[str], *, unlocked: bool = True) -> None:
    assert len(given.wallets) == len(valid)
    for given_wallet in given.wallets:
        assert given_wallet.name in valid
        assert given_wallet.unlocked == unlocked


@pytest.mark.parametrize("wallet_name", ["test", "123"])
def test_create_wallet(beekeeper: Beekeeper, wallet_name: str) -> None:
    # ARRANGE & ACT
    beekeeper.api.beekeeper.create(wallet_name=wallet_name)

    # ASSERT
    check_wallets(beekeeper.api.beekeeper.list_wallets(), [wallet_name])


@pytest.mark.parametrize("invalid_wallet_name", [(",,,", "*", "   a   ", " ", "", json.dumps({"a": None, "b": 21.37}))])
def test_invalid_wallet_names(beekeeper: Beekeeper, invalid_wallet_name: str) -> None:
    # ARRANGE, ACT & ASSERT
    with pytest.raises(RequestError):
        beekeeper.api.beekeeper.create(wallet_name=invalid_wallet_name)


def test_wallet_open(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    beekeeper.restart()  # this will close

    # ACT & ASSERT
    check_wallets(beekeeper.api.beekeeper.list_wallets(), [])
    beekeeper.api.beekeeper.open(wallet_name=wallet.name)
    check_wallets(beekeeper.api.beekeeper.list_wallets(), [wallet.name], unlocked=False)


def test_wallet_unlock(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    beekeeper.api.beekeeper.lock_all()  # after creation wallet is opened and unlocked by default

    # ACT
    beekeeper.api.beekeeper.unlock(wallet_name=wallet.name, password=wallet.password)

    # ASSERT
    check_wallets(beekeeper.api.beekeeper.list_wallets(), [wallet.name])


def test_timeout(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    timeout: Final[int] = 5
    comparison_error_max_delta: Final[float] = 1.0

    # ARRANGE
    beekeeper.api.beekeeper.set_timeout(seconds=timeout)

    # ASSERT
    info = beekeeper.api.beekeeper.get_info()
    assert timeout - (info.timeout_time - info.now).total_seconds() <= comparison_error_max_delta
    check_wallets(beekeeper.api.beekeeper.list_wallets(), [wallet.name])

    # ACT
    time.sleep(timeout)

    # ASSERT
    check_wallets(beekeeper.api.beekeeper.list_wallets(), [wallet.name], unlocked=False)


@pytest.mark.parametrize("wallet_name", ["test", "123"])
def test_create_wallet_with_custom_password(beekeeper: Beekeeper, wallet_name: str) -> None:
    # ARRANGE & ACT
    password = (beekeeper.api.beekeeper.create(wallet_name=wallet_name, password=wallet_name)).password

    # ASSERT
    assert password == wallet_name
