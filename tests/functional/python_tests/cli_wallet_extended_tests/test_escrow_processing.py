import test_tools as tt

from .utilities import create_accounts


def test_escrow(wallet):
    wallet.api.create_account("initminer", "alice", "{}")

    wallet.api.transfer("initminer", "alice", tt.Asset.Test(200), "avocado")

    wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "banana")

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(50))

    create_accounts(wallet, "alice", ["bob", "carol"])

    wallet.api.transfer("alice", "bob", tt.Asset.Test(50), "lemon")

    wallet.api.transfer_to_vesting("alice", "bob", tt.Asset.Test(25))

    _result = wallet.api.get_accounts(["alice", "bob", "carol"])
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice["balance"] == tt.Asset.Test(125)
    assert _alice["hbd_balance"] == tt.Asset.Tbd(100)

    _bob = _result[1]
    assert _bob["balance"] == tt.Asset.Test(50)
    assert _bob["hbd_balance"] == tt.Asset.Tbd(0)

    _carol = _result[2]
    assert _carol["balance"] == tt.Asset.Test(0)
    assert _carol["hbd_balance"] == tt.Asset.Tbd(0)

    wallet.api.escrow_transfer(
        "alice",
        "bob",
        "carol",
        99,
        tt.Asset.Tbd(1),
        tt.Asset.Test(2),
        tt.Asset.Tbd(5),
        "2029-06-02T00:00:00",
        "2029-06-02T01:01:01",
        "{}",
    )

    _result = wallet.api.get_accounts(["alice", "bob", "carol"])
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice["balance"] == tt.Asset.Test(123)
    assert _alice["hbd_balance"] == tt.Asset.Tbd(94)

    _bob = _result[1]
    assert _bob["balance"] == tt.Asset.Test(50)
    assert _bob["hbd_balance"] == tt.Asset.Tbd(0)

    _carol = _result[2]
    assert _carol["balance"] == tt.Asset.Test(0)
    assert _carol["hbd_balance"] == tt.Asset.Tbd(0)

    wallet.api.transfer_to_vesting("initminer", "carol", tt.Asset.Test(50))

    wallet.api.escrow_approve("alice", "bob", "carol", "carol", 99, True)

    _result = wallet.api.get_accounts(["alice", "bob", "carol"])
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice["balance"] == tt.Asset.Test(123)
    assert _alice["hbd_balance"] == tt.Asset.Tbd(94)

    _bob = _result[1]
    assert _bob["balance"] == tt.Asset.Test(50)
    assert _bob["hbd_balance"] == tt.Asset.Tbd(0)

    _carol = _result[2]
    assert _carol["balance"] == tt.Asset.Test(0)
    assert _carol["hbd_balance"] == tt.Asset.Tbd(0)

    wallet.api.escrow_approve("alice", "bob", "carol", "bob", 99, True)

    _result = wallet.api.get_accounts(["alice", "bob", "carol"])
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice["balance"] == tt.Asset.Test(123)
    assert _alice["hbd_balance"] == tt.Asset.Tbd(94)

    _bob = _result[1]
    assert _bob["balance"] == tt.Asset.Test(50)
    assert _bob["hbd_balance"] == tt.Asset.Tbd(0)

    _carol = _result[2]
    assert _carol["balance"] == tt.Asset.Test(0)
    assert _carol["hbd_balance"] == tt.Asset.Tbd(5)

    wallet.api.escrow_dispute("alice", "bob", "carol", "alice", 99)

    _result = wallet.api.get_accounts(["alice", "bob", "carol"])
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice["balance"] == tt.Asset.Test(123)
    assert _alice["hbd_balance"] == tt.Asset.Tbd(94)

    _bob = _result[1]
    assert _bob["balance"] == tt.Asset.Test(50)
    assert _bob["hbd_balance"] == tt.Asset.Tbd(0)

    _carol = _result[2]
    assert _carol["balance"] == tt.Asset.Test(0)
    assert _carol["hbd_balance"] == tt.Asset.Tbd(5)

    wallet.api.escrow_release("alice", "bob", "carol", "carol", "bob", 99, tt.Asset.Tbd(1), tt.Asset.Test(2))

    _result = wallet.api.get_accounts(["alice", "bob", "carol"])
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice["balance"] == tt.Asset.Test(123)
    assert _alice["hbd_balance"] == tt.Asset.Tbd(94)

    _bob = _result[1]
    assert _bob["balance"] == tt.Asset.Test(52)
    assert _bob["hbd_balance"] == tt.Asset.Tbd(1)

    _carol = _result[2]
    assert _carol["balance"] == tt.Asset.Test(0)
    assert _carol["hbd_balance"] == tt.Asset.Tbd(5)
