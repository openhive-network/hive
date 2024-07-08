from __future__ import annotations

from concurrent.futures import ProcessPoolExecutor
from typing import TYPE_CHECKING

from beekeepy import PackedBeekeeper, beekeeper_factory

if TYPE_CHECKING:
    from hive_local_tools.beekeeper.models import SettingsFactory


def import_new_key(packed: PackedBeekeeper, wallet: str, password: str) -> None:
    with packed.unpack() as bk, bk.create_session() as ss, ss.open_wallet(name=wallet).unlock(password=password) as wlt:
        wlt.generate_key()


def test_packing(settings: SettingsFactory) -> None:
    # ARRANGE
    name, password = "wallet", "password"
    with beekeeper_factory(settings=settings()) as bk, bk.create_session() as ss, ss.create_wallet(
        name=name, password=password
    ) as wlt:
        assert len(wlt.public_keys) == 0, "Unexpected public keys in wallet"
        with ProcessPoolExecutor() as executor:
            # ACT
            packed_beekeeper = bk.pack()
            future = executor.submit(import_new_key, packed_beekeeper, name, password)
            exception = future.exception()

            # ASSERT
            assert exception is None, str(exception)
