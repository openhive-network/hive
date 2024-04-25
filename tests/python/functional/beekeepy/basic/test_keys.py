from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING, Final

import pytest

from clive.__private.core.beekeeper.model import BeekeeperPublicKeyType, ImportKey

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive.__private.core.keys.keys import PrivateKey, PublicKey
    from clive_local_tools.data.models import WalletInfo

PRIVATE_AND_PUBLIC_KEYS: Final[list[tuple[str, str]]] = [
    (
        "5HwHC7y2WtCL18J9QMqX7awDe1GDsUTg7cfw734m2qFkdMQK92q",
        "6jACfK3P5xYFJQvavCwz5M8KR5EW3TcmSesArj9LJVGAq85qor",
    ),
    (
        "5J7m49WCKnRBTo1HyJisBinn8Lk3syYaXsrzdFmfDxkejHLwZ1m",
        "5hjCkhcMKcXQMppa97XUbDR5dWC3c2K8h23P1ajEi2fT9YuagW",
    ),
    (
        "5JqStPwQgnXBdyRDDxCDyjfC8oNjgJuZsBkT6MMz6FopydAebbC",
        "5EjyFcCNidBSivAtTKvWWwWRNjakcRMB79QrbwMKprcTRBHtXz",
    ),
    (
        "5JowdvuiDxoeLhzoSEKK74TCiwTaUHvxtRH3fkbweVEJZEsQJoc",
        "8RgQ3yexUZjcVGxQ2i3cKywwKwhxqzwtCHPQznGUYvQ15ZvahW",
    ),
    (
        "5Khrc9PX4S8wAUUmX4h2JpBgf4bhvPyFT5RQ6tGfVpKEudwpYjZ",
        "77P1n96ojdXcKpd5BRUhVmk7qFTfM2q2UkWSKg63Xi7NKyK2Q1",
    ),
]

AllowedListOfPublicKeysT = list[BeekeeperPublicKeyType] | list[ImportKey] | list[str]


def assert_keys(given: AllowedListOfPublicKeysT, valid: AllowedListOfPublicKeysT) -> None:
    def normalize(keys: AllowedListOfPublicKeysT) -> list[str]:
        def convert(key: BeekeeperPublicKeyType | ImportKey | str) -> str:
            processed_key = key.public_key if not isinstance(key, str) else key

            if processed_key.startswith(("TST", "STM")):
                return processed_key[3:]
            return processed_key

        return sorted([convert(key) for key in keys])

    assert normalize(valid) == normalize(given)


@pytest.mark.parametrize("prv_pub", PRIVATE_AND_PUBLIC_KEYS)
async def test_key_import(beekeeper: Beekeeper, prv_pub: tuple[str, str], wallet_no_keys: WalletInfo) -> None:
    # ARRANGE & ACT
    pub_key = (await beekeeper.api.import_key(wallet_name=wallet_no_keys.name, wif_key=prv_pub[0])).public_key

    # ASSERT
    assert_keys([pub_key], [prv_pub[1]])
    assert_keys((await beekeeper.api.get_public_keys()).keys, [prv_pub[1]])


async def test_import_multiple_keys(beekeeper: Beekeeper, wallet_no_keys: WalletInfo) -> None:
    # ARRANGE & ACT
    import_keys_result: list[ImportKey] = await asyncio.gather(
        *[beekeeper.api.import_key(wallet_name=wallet_no_keys.name, wif_key=prv) for prv, _ in PRIVATE_AND_PUBLIC_KEYS]
    )
    public_keys = [x.public_key for x in import_keys_result]
    received_public_keys = (await beekeeper.api.get_public_keys()).keys

    # ASSERT
    assert_keys(received_public_keys, public_keys)


async def test_remove_key(
    beekeeper: Beekeeper, wallet_no_keys: WalletInfo, key_pair: tuple[PublicKey, PrivateKey]
) -> None:
    # ARRANGE
    public_key, private_key = key_pair
    await beekeeper.api.import_key(wallet_name=wallet_no_keys.name, wif_key=private_key.value)

    # ACT & ASSERT
    assert_keys((await beekeeper.api.get_public_keys()).keys, [public_key.value])
    await beekeeper.api.remove_key(
        wallet_name=wallet_no_keys.name, password=wallet_no_keys.password, public_key=public_key.value
    )
    assert_keys((await beekeeper.api.get_public_keys()).keys, [])
