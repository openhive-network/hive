from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing import Final

    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo


def test_sign_digest(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    private_key: Final[str] = "5HwHC7y2WtCL18J9QMqX7awDe1GDsUTg7cfw734m2qFkdMQK92q"
    public_key: Final[str] = "6jACfK3P5xYFJQvavCwz5M8KR5EW3TcmSesArj9LJVGAq85qor"
    digest_to_sign: Final[str] = "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff"
    expected_signature: Final[str] = (
        "1f481d8a164af3f4de957aee236ca1f673825839534912d87e638f0695096718e006ae334f21141ee4a7df5170512fde64faa2123bb2cfc4070539e81b4fab9c6e"
    )

    # ARRANGE
    beekeeper.api.beekeeper.import_key(wallet_name=wallet.name, wif_key=private_key)

    # ACT
    signature = beekeeper.api.beekeeper.sign_digest(sig_digest=digest_to_sign, public_key=public_key).signature

    # ASSERT
    assert expected_signature == signature
