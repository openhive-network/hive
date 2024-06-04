from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing import Final

    from beekeepy._handle import Beekeeper
    from hive_local_tools.beekeeper.models import WalletInfo


def test_sign_digest(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    private_key: Final[str] = "5HwHC7y2WtCL18J9QMqX7awDe1GDsUTg7cfw734m2qFkdMQK92q"
    public_key: Final[str] = "STM6jACfK3P5xYFJQvavCwz5M8KR5EW3TcmSesArj9LJVGAq85qor"
    digest_to_sign: Final[str] = "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff"
    expected_signature: Final[str] = (
        "20e25ced7114f8edc36127453c97b2b78884611896701a02b018d977e707ca7a1e4c82a9997a520890b35ed1ecb87ddd66190735e126e3f2b2329d12059af1f3e9"
    )

    # ARRANGE
    beekeeper.api.import_key(wallet_name=wallet.name, wif_key=private_key)

    # ACT
    signature = beekeeper.api.sign_digest(sig_digest=digest_to_sign, public_key=public_key).signature

    # ASSERT
    assert expected_signature == signature
