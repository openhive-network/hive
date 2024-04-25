from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo


async def test_api_has_matching_key(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ACT
    result = await beekeeper.api.has_matching_private_key(wallet_name=wallet.name, public_key=wallet.public_key.value)

    # ASSERT
    assert result, "Key should be found in wallet."
