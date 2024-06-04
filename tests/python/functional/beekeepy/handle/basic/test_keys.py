from __future__ import annotations

from typing import TYPE_CHECKING, Final

import pytest

from schemas.apis.beekeeper_api.response_schemas import ImportKey, PublicKeyItem
from schemas.fields.basic import PrivateKey, PublicKey

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper
    from hive_local_tools.beekeeper.models import WalletInfo

PRIVATE_AND_PUBLIC_KEYS: Final[list[tuple[str, str]]] = [
    (
        "5HwHC7y2WtCL18J9QMqX7awDe1GDsUTg7cfw734m2qFkdMQK92q",
        "STM6jACfK3P5xYFJQvavCwz5M8KR5EW3TcmSesArj9LJVGAq85qor",
    ),
    (
        "5J7m49WCKnRBTo1HyJisBinn8Lk3syYaXsrzdFmfDxkejHLwZ1m",
        "STM5hjCkhcMKcXQMppa97XUbDR5dWC3c2K8h23P1ajEi2fT9YuagW",
    ),
    (
        "5JqStPwQgnXBdyRDDxCDyjfC8oNjgJuZsBkT6MMz6FopydAebbC",
        "STM5EjyFcCNidBSivAtTKvWWwWRNjakcRMB79QrbwMKprcTRBHtXz",
    ),
    (
        "5JowdvuiDxoeLhzoSEKK74TCiwTaUHvxtRH3fkbweVEJZEsQJoc",
        "STM8RgQ3yexUZjcVGxQ2i3cKywwKwhxqzwtCHPQznGUYvQ15ZvahW",
    ),
    (
        "5Khrc9PX4S8wAUUmX4h2JpBgf4bhvPyFT5RQ6tGfVpKEudwpYjZ",
        "STM77P1n96ojdXcKpd5BRUhVmk7qFTfM2q2UkWSKg63Xi7NKyK2Q1",
    ),
]

AllowedListOfPublicKeysT = list[PublicKey] | list[PublicKeyItem] | list[ImportKey] | list[str] | None


def assert_keys(given: AllowedListOfPublicKeysT, valid: AllowedListOfPublicKeysT = None) -> None:
    def normalize(keys: AllowedListOfPublicKeysT) -> list[str]:
        def convert(key: PublicKey | PublicKeyItem | ImportKey | str) -> str:
            processed_key = key.public_key if not isinstance(key, str) else key

            if processed_key.startswith(("TST", "STM")):
                return processed_key[3:]
            return processed_key

        if keys is None:
            return []
        return sorted([convert(key) for key in keys])

    assert normalize(valid) == normalize(given)


@pytest.mark.parametrize("prv_pub", PRIVATE_AND_PUBLIC_KEYS)
def test_key_import(beekeeper: Beekeeper, prv_pub: tuple[str, str], wallet: WalletInfo) -> None:
    # ARRANGE & ACT
    pub_key = (beekeeper.api.import_key(wallet_name=wallet.name, wif_key=prv_pub[0])).public_key

    # ASSERT
    assert_keys([pub_key], [prv_pub[1]])
    assert_keys(beekeeper.api.get_public_keys().keys, [prv_pub[1]])


def test_import_multiple_keys(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE & ACT
    import_keys_result: list[ImportKey] = [
        beekeeper.api.import_key(wallet_name=wallet.name, wif_key=prv) for prv, _ in PRIVATE_AND_PUBLIC_KEYS
    ]
    public_keys = [x.public_key for x in import_keys_result]
    received_public_keys = beekeeper.api.get_public_keys().keys

    # ASSERT
    assert_keys(received_public_keys, public_keys)


@pytest.mark.parametrize("key_pair", PRIVATE_AND_PUBLIC_KEYS)
def test_remove_key(
    beekeeper: Beekeeper,
    wallet: WalletInfo,
    key_pair: tuple[PublicKey, PrivateKey],
) -> None:
    # ARRANGE
    private_key, public_key = [PrivateKey(key_pair[0]), PublicKey(key_pair[1])]
    beekeeper.api.import_key(wallet_name=wallet.name, wif_key=private_key)

    # ACT & ASSERT
    assert_keys((beekeeper.api.get_public_keys()).keys, [public_key])
    beekeeper.api.remove_key(
        wallet_name=wallet.name,
        password=wallet.password,
        public_key=public_key,
    )
    assert_keys((beekeeper.api.get_public_keys()).keys)
