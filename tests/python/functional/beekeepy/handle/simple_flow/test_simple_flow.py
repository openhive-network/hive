from __future__ import annotations

import json
import shutil
import time
from dataclasses import dataclass
from pathlib import Path

import pytest
from beekeepy._handle import Beekeeper
from beekeepy.settings import Settings
from helpy import KeyPair

import test_tools as tt
import wax
from hive_local_tools.beekeeper.constants import DIGEST_TO_SIGN
from hive_local_tools.beekeeper.generators import (
    generate_wallet_name,
    generate_wallet_password,
)
from hive_local_tools.beekeeper.models import WalletInfo
from schemas.fields.basic import PrivateKey, PublicKey


@dataclass
class WalletInfoWithKeysToImport(WalletInfo):
    keys: list[KeyPair]

    @classmethod
    def import_from_file(cls, name: str, password: str, path: Path | None = None) -> WalletInfoWithKeysToImport:
        result = WalletInfoWithKeysToImport(name=name, password=password, keys=[])
        if path is None:
            return result

        assert path.is_dir()
        path = path / f"{name}.alias.keys"

        list_of_private_keys: list[PrivateKey] = []
        with path.open("r") as file:
            list_of_private_keys = json.load(file)

        for private_key in list_of_private_keys:
            wax_result = wax.calculate_public_key(private_key.encode("ascii"))
            assert wax_result.status == wax.python_error_code.ok
            result.keys.append(
                KeyPair(
                    public_key=PublicKey("STM" + wax_result.result.decode("ascii")),
                    private_key=PrivateKey(private_key),
                )
            )
        return result


PrepereWalletDirResultT = tuple[list[WalletInfoWithKeysToImport], Path, bool]


@pytest.fixture()
def prepare_wallet_dir(
    request: pytest.FixtureRequest,
    # number_of_wallets: int, use_existing_wallets: bool
) -> PrepereWalletDirResultT:
    """Copy wallets (.wallet) files from source_directory into number_of_dirs temp dirs."""
    number_of_wallets: int
    use_existing_wallets: bool
    number_of_wallets, use_existing_wallets = request.param

    source_directory = (Path(__file__).parent / "prepared_wallets") if use_existing_wallets else None
    source_directory_keys: Path | None = None
    dir_to_create = tt.context.get_current_directory() / f"beekeeper-{number_of_wallets}-{use_existing_wallets}"
    if dir_to_create.exists():
        shutil.rmtree(dir_to_create)
    dir_to_create.mkdir()

    if source_directory is not None:
        # Copy wallets/keys to the target temp directories, so that all beekeepers
        # will have its own source of wallets.
        source_directory_wallets = source_directory / "wallets"
        source_directory_keys = source_directory / "keys"
        for source_path in source_directory_wallets.glob("*.wallet"):
            destination_path = dir_to_create / source_path.name
            shutil.copy2(source_path, destination_path)

    wallets = [
        WalletInfoWithKeysToImport.import_from_file(
            name=generate_wallet_name(i),
            password=generate_wallet_password(i),
            path=source_directory_keys,
        )
        for i in range(5)
    ]
    return wallets, dir_to_create, use_existing_wallets


@pytest.fixture()
def wallets(
    prepare_wallet_dir: PrepereWalletDirResultT,
) -> list[WalletInfoWithKeysToImport]:
    return prepare_wallet_dir[0]


@pytest.fixture()
def wallet_dir(prepare_wallet_dir: PrepereWalletDirResultT) -> Path:
    return prepare_wallet_dir[1]


@pytest.fixture()
def use_existing_wallets(prepare_wallet_dir: PrepereWalletDirResultT) -> bool:
    return prepare_wallet_dir[2]


def assert_wallet_unlocked(bk: Beekeeper, wallet_name: str) -> None:
    """Assert function checking if given wallet has been unlocked."""
    unlocked_wallets = [wallet.name for wallet in (bk.api.list_wallets()).wallets if wallet.unlocked is True]
    assert wallet_name in unlocked_wallets, "Wallet should be unlocked."


def assert_wallet_closed(bk: Beekeeper, wallet_name: str) -> None:
    """Assert function checking if given wallet has been closed."""
    opened_wallets = [wallet.name for wallet in (bk.api.list_wallets()).wallets]
    assert wallet_name not in opened_wallets, "Wallet should be closed."


def assert_wallet_opened(bk: Beekeeper, wallet_name: str) -> None:
    """Assert function checking if given wallet has been opened."""
    opened_wallets = [wallet.name for wallet in (bk.api.list_wallets()).wallets]
    assert wallet_name in opened_wallets, "Wallet should be opened."


def assert_number_of_wallets_opened(bk: Beekeeper, number_of_available_wallets: int) -> None:
    """Assert function checking if bk has required number of opened wallets in current session."""
    bk_wallets = (bk.api.list_wallets()).wallets
    assert number_of_available_wallets == len(
        bk_wallets
    ), f"There should be {number_of_available_wallets} opened wallets, but there are {len(bk_wallets)}."


def assert_same_keys(bk: Beekeeper, wallet: WalletInfoWithKeysToImport) -> None:
    """Assert function checkinf if bk holds the same keys, as given wallet."""
    bk_keys = sorted([keys.public_key for keys in (bk.api.get_public_keys()).keys])
    wallet_keys = sorted([key_pair.public_key for key_pair in wallet.keys])
    assert bk_keys == wallet_keys, "There should be same keys."


def assert_keys_empty(bk: Beekeeper) -> None:
    """Assert function checking if bk holds no public keys."""
    bk_keys_empty = (bk.api.get_public_keys()).keys
    assert len(bk_keys_empty) == 0, "There should be no keys."


def create_wallets_if_needed(
    bk: Beekeeper,
    wallet: WalletInfoWithKeysToImport,
    use_existing_wallets: bool,
) -> None:
    """Helper function for creating wallets under specific terms."""
    # Do not create new wallets, if there is already existing one
    if use_existing_wallets:
        return

    # Create given wallet
    bk.api.create(wallet_name=wallet.name, password=wallet.password)
    for keys in wallet.keys:
        bk.api.import_key(wallet_name=wallet.name, wif_key=keys.private_key)


@pytest.mark.parametrize(
    "prepare_wallet_dir",
    [[1, True], [2, True], [1, False], [2, False]],
    indirect=True,
)
def test_simple_flow(
    wallets: list[WalletInfoWithKeysToImport],
    wallet_dir: Path,
    use_existing_wallets: bool,
) -> None:
    """
    Test simple flow of one or multiply instance of beekeepers launched parallel.

    * Creating/deleting session(s),
    * Creating/deleting wallet(s),
    * Locking/unlockinq wallet(s),
    * Check timeout,
    * Creating/removing/listing keys,
    * List wallet(s),
    * Signing.
    """
    with Beekeeper(settings=Settings(working_directory=wallet_dir), logger=tt.logger) as beekeeper:
        # ACT & ASSERT 1
        # In this block we will create new session, and wallet import key to is and sign a digest
        for wallet_nr, wallet in enumerate(wallets):
            create_wallets_if_needed(beekeeper, wallet, use_existing_wallets)

            beekeeper.api.open(wallet_name=wallet.name)
            beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)

            assert_wallet_opened(beekeeper, wallet.name)
            assert_wallet_unlocked(beekeeper, wallet.name)
            assert_number_of_wallets_opened(beekeeper, wallet_nr + 1)
            assert_same_keys(beekeeper, wallet)

            for keys in wallet.keys:
                beekeeper.api.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=keys.public_key)
            beekeeper.api.lock(wallet_name=wallet.name)
        assert_number_of_wallets_opened(beekeeper, len(wallets))

        # ACT & ASSERT 2
        # In this block we will unlock previous locked wallet, get public keys, list walleta, remove key from unlocked wallet and set timeout on it.
        for wallet in wallets:
            beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
            assert_wallet_unlocked(beekeeper, wallet.name)
            assert_number_of_wallets_opened(beekeeper, len(wallets))
            assert_same_keys(beekeeper, wallet)
            for keys in wallet.keys:
                beekeeper.api.remove_key(
                    wallet_name=wallet.name,
                    password=wallet.password,
                    public_key=keys.public_key,
                )
        assert_keys_empty(beekeeper)
        assert_number_of_wallets_opened(beekeeper, len(wallets))
        beekeeper.api.set_timeout(seconds=1)

        # ACT & ASSERT 3
        # In this block we will unlock wallet which should be locked by timeout, close it, and lastly close all sessions.
        time.sleep(2)
        for wallet_nr, wallet in enumerate(wallets):
            beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
            assert_wallet_unlocked(beekeeper, wallet.name)
            assert_number_of_wallets_opened(beekeeper, len(wallets) - wallet_nr)

            beekeeper.api.close(wallet_name=wallet.name)
            assert_wallet_closed(beekeeper, wallet.name)
        assert_number_of_wallets_opened(beekeeper, 0)
