from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", enable_plugins=["account_history_api"])
def test_getting_empty_history(wallet: tt.OldWallet) -> None:
    assert len(wallet.api.get_account_history("initminer", -1, 1)) == 0
