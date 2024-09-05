from __future__ import annotations

import json
from typing import TYPE_CHECKING, Final

from beekeepy._handle import Beekeeper
from helpy import KeyPair

if TYPE_CHECKING:
    from pathlib import Path

PRIVATE_AND_PUBLIC_KEYS: Final[list[KeyPair]] = [
    KeyPair(
        private_key="5JqStPwQgnXBdyRDDxCDyjfC8oNjgJuZsBkT6MMz6FopydAebbC",
        public_key="STM5EjyFcCNidBSivAtTKvWWwWRNjakcRMB79QrbwMKprcTRBHtXz",
    ),
    KeyPair(
        private_key="5J7m49WCKnRBTo1HyJisBinn8Lk3syYaXsrzdFmfDxkejHLwZ1m",
        public_key="STM5hjCkhcMKcXQMppa97XUbDR5dWC3c2K8h23P1ajEi2fT9YuagW",
    ),
    KeyPair(
        private_key="5HwHC7y2WtCL18J9QMqX7awDe1GDsUTg7cfw734m2qFkdMQK92q",
        public_key="STM6jACfK3P5xYFJQvavCwz5M8KR5EW3TcmSesArj9LJVGAq85qor",
    ),
    KeyPair(
        private_key="5Khrc9PX4S8wAUUmX4h2JpBgf4bhvPyFT5RQ6tGfVpKEudwpYjZ",
        public_key="STM77P1n96ojdXcKpd5BRUhVmk7qFTfM2q2UkWSKg63Xi7NKyK2Q1",
    ),
    KeyPair(
        private_key="5JowdvuiDxoeLhzoSEKK74TCiwTaUHvxtRH3fkbweVEJZEsQJoc",
        public_key="STM8RgQ3yexUZjcVGxQ2i3cKywwKwhxqzwtCHPQznGUYvQ15ZvahW",
    ),
]


def check_dumped_keys(wallet_path: Path, extracted_keys: list[KeyPair]) -> None:
    """Check if keys has been saved in dumped {wallet_name}.keys file."""
    with wallet_path.open() as keys_file:
        keys = json.load(keys_file)
        assert extracted_keys == keys
        assert extracted_keys == PRIVATE_AND_PUBLIC_KEYS

    wallet_path.unlink()


def test_export_keys(beekeeper: Beekeeper) -> None:
    """Test will check command line flag --export-keys-wallet-name --export-keys-wallet-password."""
    # ARRANGE
    wallet_name = "test_export_keys"
    wallet_name_keys = f"{wallet_name}.keys"
    nondefault_wallet_name_keys = f"nondefault-{wallet_name_keys}"

    extract_path = beekeeper.settings.working_directory

    create = beekeeper.api.create(wallet_name=wallet_name)
    beekeeper.api.open(wallet_name=wallet_name)
    beekeeper.api.unlock(password=create.password, wallet_name=wallet_name)

    for key_pair in PRIVATE_AND_PUBLIC_KEYS:
        beekeeper.api.import_key(wif_key=key_pair.private_key, wallet_name=wallet_name)

    # ACT
    keys = beekeeper.export_keys_wallet(
        wallet_name=wallet_name,
        wallet_password=create.password,
        extract_to=extract_path / nondefault_wallet_name_keys,
    )
    # ASSERT
    # Check extract_to path
    check_dumped_keys(extract_path / nondefault_wallet_name_keys, keys)

    # ACT
    keys = beekeeper.export_keys_wallet(wallet_name=wallet_name, wallet_password=create.password)
    # ASSERT
    # Check default path of wallet_name.keys
    check_dumped_keys(extract_path / wallet_name_keys, keys)

    # ARRANGE
    bk = Beekeeper(settings=beekeeper.settings, logger=beekeeper.logger)

    # ACT
    keys1 = bk.export_keys_wallet(
        wallet_name=wallet_name,
        wallet_password=create.password,
        extract_to=extract_path / nondefault_wallet_name_keys,
    )
    # ASSERT
    # Check extract_to path
    check_dumped_keys(extract_path / nondefault_wallet_name_keys, keys1)

    # ACT
    keys1 = bk.export_keys_wallet(wallet_name=wallet_name, wallet_password=create.password)
    # ASSERT
    # Check default path of wallet_name.keys
    check_dumped_keys(extract_path / wallet_name_keys, keys1)
    assert bk.is_running is False
