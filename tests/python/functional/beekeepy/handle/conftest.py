from __future__ import annotations

from functools import wraps
from typing import Iterator

import pytest
from beekeepy._handle import Beekeeper

import test_tools as tt
from hive_local_tools.beekeeper.generators import (
    generate_account_name,
    generate_wallet_name,
    generate_wallet_password,
)
from hive_local_tools.beekeeper.models import (
    SettingsLoggerFactory,
    WalletInfo,
    WalletInfoWithImportedAccounts,
    WalletsGeneratorT,
)


@pytest.fixture()
def beekeeper_not_started(settings_with_logger: SettingsLoggerFactory) -> Beekeeper:
    incoming_settings, logger = settings_with_logger()
    return Beekeeper(settings=incoming_settings, logger=logger)


@pytest.fixture()
def beekeeper(beekeeper_not_started: Beekeeper) -> Iterator[Beekeeper]:
    with beekeeper_not_started as bk:
        yield bk


@pytest.fixture()
def wallet(beekeeper: Beekeeper) -> WalletInfo:
    name, password = "wallet", "password"
    beekeeper.api.create(wallet_name=name, password=password)
    return WalletInfo(name=name, password=password)


@pytest.fixture()
def account(beekeeper: Beekeeper, wallet: WalletInfo) -> tt.Account:
    acc = tt.Account("account")
    beekeeper.api.import_key(wallet_name=wallet.name, wif_key=acc.private_key)
    return acc


@pytest.fixture(scope="session")
def keys_to_import() -> list[tt.Account]:
    return tt.Account.create_multiple(10)


@pytest.fixture()
def setup_wallets(beekeeper: Beekeeper) -> WalletsGeneratorT:
    @wraps(setup_wallets)
    def __setup_wallets(
        count: int, *, import_keys: bool = True, keys_per_wallet: int = 1
    ) -> list[WalletInfoWithImportedAccounts]:
        wallets = [
            WalletInfoWithImportedAccounts(
                name=generate_wallet_name(i),
                password=generate_wallet_password(i),
                accounts=(
                    tt.Account.create_multiple(keys_per_wallet, name_base=generate_account_name(i))
                    if keys_per_wallet
                    else []
                ),
            )
            for i in range(count)
        ]
        assert len(wallets) == count, "Incorrect number of created wallets"
        with beekeeper.batch() as bk:
            for wallet in wallets:
                bk.api.beekeeper.create(wallet_name=wallet.name, password=wallet.password)

                if import_keys:
                    for account in wallet.accounts:
                        bk.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=account.private_key)
        return wallets

    return __setup_wallets
