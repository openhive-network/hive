from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo


def test_api_has_matching_key(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    # ARRANGE
    account = tt.Account("alice")
    beekeeper.api.import_key(wallet_name=wallet.name, wif_key=account.private_key)

    # ACT
    result = beekeeper.api.has_matching_private_key(wallet_name=wallet.name, public_key=account.public_key)

    # ASSERT
    assert result, "Key should be found in wallet."
