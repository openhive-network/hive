from __future__ import annotations

import test_tools as tt

from .utilities import create_accounts


def test_transfer(node: tt.InitNode, wallet: tt.Wallet) -> None:
    create_accounts(wallet, "initminer", ["newaccount", "newaccount2"])

    wallet.api.transfer_to_vesting("initminer", "newaccount", tt.Asset.Test(100))

    _result = wallet.api.get_account("newaccount")
    assert _result["balance"] == tt.Asset.Test(0)
    assert _result["hbd_balance"] == tt.Asset.Tbd(0)
    assert _result["savings_balance"] == tt.Asset.Test(0)
    assert _result["savings_hbd_balance"] == tt.Asset.Tbd(0)
    assert _result["vesting_shares"] != tt.Asset.Vest(0)

    wallet.api.transfer("initminer", "newaccount", tt.Asset.Test(5.432), "banana")

    wallet.api.transfer("initminer", "newaccount", tt.Asset.Tbd(9.169), "banana2")

    wallet.api.transfer_to_savings("initminer", "newaccount", tt.Asset.Test(15.432), "pomelo")

    wallet.api.transfer_to_savings("initminer", "newaccount", tt.Asset.Tbd(19.169), "pomelo2")

    _result = wallet.api.get_account("newaccount")
    assert _result["balance"] == tt.Asset.Test(5.432)
    assert _result["hbd_balance"] == tt.Asset.Tbd(9.169)
    assert _result["savings_balance"] == tt.Asset.Test(15.432)
    assert _result["savings_hbd_balance"] == tt.Asset.Tbd(19.169)

    wallet.api.transfer_from_savings("newaccount", 7, "newaccount2", tt.Asset.Test(0.001), "kiwi")

    wallet.api.transfer_from_savings("newaccount", 8, "newaccount2", tt.Asset.Tbd(0.001), "kiwi2")

    _result = wallet.api.get_account("newaccount")
    assert _result["balance"] == tt.Asset.Test(5.432)
    assert _result["hbd_balance"] == tt.Asset.Tbd(9.169)
    assert _result["savings_balance"] == tt.Asset.Test(15.431)
    assert _result["savings_hbd_balance"] == tt.Asset.Tbd(19.168)

    response = wallet.api.transfer_nonblocking("newaccount", "newaccount2", tt.Asset.Test(0.1), "mango")

    wallet.api.transfer_nonblocking("newaccount", "newaccount2", tt.Asset.Tbd(0.2), "mango2")

    wallet.api.transfer_to_vesting_nonblocking("initminer", "newaccount", tt.Asset.Test(100))

    tt.logger.info("Waiting...")
    node.wait_number_of_blocks(1)

    _result = wallet.api.get_account("newaccount")
    assert _result["balance"] == tt.Asset.Test(5.332)
    assert _result["hbd_balance"] == tt.Asset.Tbd(8.969)
    assert _result["savings_balance"] == tt.Asset.Test(15.431)
    assert _result["vesting_shares"] != tt.Asset.Vest(0)

    wallet.api.cancel_transfer_from_savings("newaccount", 7)

    response = wallet.api.get_account("newaccount")

    assert response["savings_balance"] == tt.Asset.Test(15.432)
