from __future__ import annotations

import asyncio
import shutil
from pathlib import Path

import pytest

from clive.__private.core.beekeeper import Beekeeper
from clive_local_tools.beekeeper.constants import DIGEST_TO_SIGN
from clive_local_tools.data.generates import generate_wallet_name, generate_wallet_password
from clive_local_tools.data.models import Keys, WalletInfo


async def prepare_wallet_dirs(
    tmp_path: Path,
    number_of_dirs: int,
    source_directory: Path | None = None,
) -> tuple[list[WalletInfo], list[Path]]:
    """Copy wallets (.wallet) files from source_directory into number_of_dirs temp dirs."""
    temp_directories = [tmp_path / f"wallets-{n}" for n in range(number_of_dirs)]

    for tmp_dir in temp_directories:
        tmp_dir.mkdir()

    source_directory_keys = None
    if source_directory:
        # Copy wallets/keys to the target temp directories, so that all beekeepers
        # will have its own source of wallets.
        source_directory_wallets = source_directory / "wallets"
        source_directory_keys = source_directory / "keys"
        wallet_files = [f.name for f in source_directory_wallets.glob("*.wallet")]
        for wallet_file in wallet_files:
            for temp_dir in temp_directories:
                source_path = source_directory_wallets / wallet_file
                destination_path = temp_dir / wallet_file
                shutil.copy2(source_path, destination_path)
    wallets = [
        WalletInfo(
            generate_wallet_password(i),
            generate_wallet_name(i),
            Keys() if source_directory_keys else Keys(count=i % 5),
            source_directory_keys / f"{generate_wallet_name(i)}.alias.keys" if source_directory_keys else None,
        )
        for i in range(5)
    ]
    return wallets, temp_directories


async def assert_wallet_unlocked(bk: Beekeeper, wallet_name: str) -> None:
    """Assert function checking if given wallet has been unlocked."""
    unlocked_wallets = [wallet.name for wallet in (await bk.api.list_wallets()).wallets if wallet.unlocked is True]
    assert wallet_name in unlocked_wallets, "Wallet should be unlocked."


async def assert_wallet_closed(bk: Beekeeper, wallet_name: str) -> None:
    """Assert function checking if given wallet has been closed."""
    opened_wallets = [wallet.name for wallet in (await bk.api.list_wallets()).wallets]
    assert wallet_name not in opened_wallets, "Wallet should be closed."


async def assert_wallet_opened(bk: Beekeeper, wallet_name: str) -> None:
    """Assert function checking if given wallet has been opened."""
    opened_wallets = [wallet.name for wallet in (await bk.api.list_wallets()).wallets]
    assert wallet_name in opened_wallets, "Wallet should be opened."


async def assert_number_of_wallets_opened(bk: Beekeeper, number_of_available_wallets: int) -> None:
    """Assert function checking if bk has required number of opened wallets in current session."""
    bk_wallets = (await bk.api.list_wallets()).wallets
    assert number_of_available_wallets == len(
        bk_wallets
    ), f"There should be {number_of_available_wallets} opened wallets, but there are {len(bk_wallets)}."


async def assert_same_keys(bk: Beekeeper, wallet: WalletInfo) -> None:
    """Assert function checkinf if bk holds the same keys, as given wallet."""
    bk_keys = sorted([keys.public_key for keys in (await bk.api.get_public_keys()).keys])
    wallet_keys = wallet.keys.get_public_keys()
    assert bk_keys == wallet_keys, "There should be same keys."


async def assert_keys_empty(bk: Beekeeper) -> None:
    """Assert function checking if bk holds no public keys."""
    bk_keys_empty = (await bk.api.get_public_keys()).keys
    assert len(bk_keys_empty) == 0, "There should be no keys."


async def create_wallets_if_needed(
    bk: Beekeeper,
    wallet: WalletInfo,
    use_existing_wallets: bool,
) -> None:
    """Helper function for creating wallets under specific terms."""
    # Do not create new wallets, if there is already existing one
    if use_existing_wallets:
        return

    # Create given wallet
    await bk.api.create(wallet_name=wallet.name, password=wallet.password)
    for keys in wallet.keys.pairs:
        await bk.api.import_key(wallet_name=wallet.name, wif_key=keys.private_key.value)


async def simple_flow(*, wallet_dir: Path, wallets: list[WalletInfo], use_existing_wallets: bool) -> None:
    async with await Beekeeper().launch(wallet_dir=wallet_dir) as bk:
        # ACT & ASSERT 1
        # In this block we will create new session, and wallet import key to is and sign a digest
        for wallet_nr, wallet in enumerate(wallets):
            await create_wallets_if_needed(bk, wallet, use_existing_wallets)

            await bk.api.open(wallet_name=wallet.name)
            await bk.api.unlock(wallet_name=wallet.name, password=wallet.password)

            await assert_wallet_opened(bk, wallet.name)
            await assert_wallet_unlocked(bk, wallet.name)
            await assert_number_of_wallets_opened(bk, wallet_nr + 1)
            await assert_same_keys(bk, wallet)

            for keys in wallet.keys.pairs:
                await bk.api.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=keys.public_key.value)
            await bk.api.lock(wallet_name=wallet.name)
        await assert_number_of_wallets_opened(bk, len(wallets))

        # ACT & ASSERT 2
        # In this block we will unlock previous locked wallet, get public keys, list walleta, remove key from unlocked wallet and set timeout on it.
        for wallet in wallets:
            await bk.api.unlock(wallet_name=wallet.name, password=wallet.password)
            await assert_wallet_unlocked(bk, wallet.name)
            await assert_number_of_wallets_opened(bk, len(wallets))
            await assert_same_keys(bk, wallet)
            for keys in wallet.keys.pairs:
                await bk.api.remove_key(
                    wallet_name=wallet.name, password=wallet.password, public_key=keys.public_key.value
                )
        await assert_keys_empty(bk)
        await assert_number_of_wallets_opened(bk, len(wallets))
        await bk.api.set_timeout(seconds=1)

        # ACT & ASSERT 3
        # In this block we will unlock wallet which should be locked by timeout, close it, and lastly close all sessions.
        await asyncio.sleep(2)
        for wallet_nr, wallet in enumerate(wallets):
            await bk.api.unlock(wallet_name=wallet.name, password=wallet.password)
            await assert_wallet_unlocked(bk, wallet.name)
            await assert_number_of_wallets_opened(bk, len(wallets) - wallet_nr)

            await bk.api.close(wallet_name=wallet.name)
            await assert_wallet_closed(bk, wallet.name)
        await assert_number_of_wallets_opened(bk, 0)


@pytest.mark.parametrize(
    ("use_existing_wallets", "number_of_beekeeper_instances"),
    [
        (True, 1),
        (True, 2),
        (False, 1),
        (False, 2),
    ],
)
async def test_simple_flow(tmp_path: Path, use_existing_wallets: bool, number_of_beekeeper_instances: int) -> None:
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
    # ARRANGE
    source_directories = Path(__file__).parent / "prepared_wallets" if use_existing_wallets else None
    wallets, wallets_dirs = await prepare_wallet_dirs(
        tmp_path=tmp_path, number_of_dirs=number_of_beekeeper_instances, source_directory=source_directories
    )

    await asyncio.gather(
        *[
            simple_flow(
                wallet_dir=wallet_dir,
                wallets=wallets,
                use_existing_wallets=use_existing_wallets,
            )
            for wallet_dir in wallets_dirs
        ]
    )
