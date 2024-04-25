from __future__ import annotations

from typing import TYPE_CHECKING, Final

import pytest
from helpy.exceptions import RequestError

from hive_local_tools.beekeeper.constants import DIGEST_TO_SIGN

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo

PRIVATE_KEY: Final[str] = "5HwHC7y2WtCL18J9QMqX7awDe1GDsUTg7cfw734m2qFkdMQK92q"
PUBLIC_KEY: Final[str] = "6jACfK3P5xYFJQvavCwz5M8KR5EW3TcmSesArj9LJVGAq85qor"
EXPECTED_SIGNATURE: Final[str] = (
    "1f481d8a164af3f4de957aee236ca1f673825839534912d87e638f0695096718e006ae334f21141ee4a7df5170512fde64faa2123bb2cfc4070539e81b4fab9c6e"
)


@pytest.mark.parametrize("explicit_wallet_name", [False, True])
def test_api_sign_digest(beekeeper: Beekeeper, wallet: WalletInfo, explicit_wallet_name: str) -> None:
    """Test test_api_sign_digest will test beekeeper_api.sign_digest api call."""
    # ARRANGE
    explicit_wallet_name_param = {"wallet_name": wallet.name} if explicit_wallet_name else {}
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=PRIVATE_KEY)

    # ACT
    signature = (
        beekeeper.api.beekeeper.sign_digest(
            sig_digest=DIGEST_TO_SIGN,
            public_key=PUBLIC_KEY,
            **explicit_wallet_name_param,
        )
    ).signature

    # ASSERT
    assert signature == EXPECTED_SIGNATURE, "Signatures should match."


def test_api_sign_digest_with_different_wallet_name(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    not_existing_wallet_name = "not-existing"
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=PRIVATE_KEY)

    # ACT & ASSERT
    with pytest.raises(
        RequestError,
        match=f"Public key {PUBLIC_KEY} not found in {not_existing_wallet_name} wallet",
    ):
        beekeeper.api.beekeeper.sign_digest(
            sig_digest=DIGEST_TO_SIGN,
            public_key=PUBLIC_KEY,
            wallet_name=not_existing_wallet_name,
        )


def test_api_sign_digest_with_deleted_key(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_sign_digest will test beekeeper_api.sign_digest will try to sign sigest with key that hase been deleted."""
    # ARRANGE
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=PRIVATE_KEY)

    signature = (beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)).signature
    assert signature == EXPECTED_SIGNATURE, "Signatures should match."

    beekeeper.api.beekeeper.remove_key(
        wallet_name=wallet.name,
        password=wallet.password,
        public_key=PUBLIC_KEY,
    )

    # ACT & ASSERT
    with pytest.raises(RequestError, match=f"Public key {PUBLIC_KEY} not found in unlocked wallets"):
        beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)


def test_api_sign_digest_with_closed_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_sign_digest will test beekeeper_api.sign_digest will try to sign sigest with key in wallet that hase been closed."""
    # ARRANGE
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=PRIVATE_KEY)

    signature = (beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)).signature
    assert signature == EXPECTED_SIGNATURE, "Signatures should match."

    beekeeper.api.beekeeper.close(wallet_name=wallet.name)

    # ACT & ASSERT
    with pytest.raises(RequestError, match=f"Public key {PUBLIC_KEY} not found in unlocked wallets"):
        beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)


def test_api_sign_digest_with_deleted_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_sign_digest will test beekeeper_api.sign_digest will try to sign digest with key in wallet that has been deleted."""
    # ARRANGE
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=PRIVATE_KEY)

    signature = (beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)).signature
    assert signature == EXPECTED_SIGNATURE, "Signatures should match."

    # ACT
    wallet_path = beekeeper.settings.working_directory / f"{wallet.name}.wallet"
    assert wallet_path.exists() is True, "Wallet should exists."
    wallet_path.unlink()
    assert wallet_path.exists() is False, "Wallet should not exists."

    # ASSERT
    signature = (beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)).signature
    assert signature == EXPECTED_SIGNATURE, "Signatures should match."


def test_api_sign_digest_with_locked_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    """Test test_api_sign_digest will test beekeeper_api.sign_digest will try to sign digest with key in wallet that has been locked."""
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=PRIVATE_KEY)

    signature = (beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)).signature
    assert signature == EXPECTED_SIGNATURE, "Signatures should match."

    beekeeper.api.beekeeper.lock(wallet_name=wallet.name)

    # ACT & ASSERT
    with pytest.raises(RequestError, match=f"Public key {PUBLIC_KEY} not found in unlocked wallets"):
        beekeeper.api.beekeeper.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=PUBLIC_KEY)
