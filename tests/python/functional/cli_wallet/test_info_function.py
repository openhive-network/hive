from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import test_tools as tt


def test_info_function(old_wallet: tt.OldWallet) -> None:
    assert len(old_wallet.api.info()) == 53, "Info have incorrect data set"
