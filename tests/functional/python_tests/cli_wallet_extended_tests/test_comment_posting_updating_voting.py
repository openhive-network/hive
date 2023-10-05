import test_tools as tt


def test_comment(wallet):
    wallet.api.create_account("initminer", "alice", "{}")

    wallet.api.transfer("initminer", "alice", tt.Asset.Test(200), "avocado")

    wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "banana")

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(50))

    wallet.api.create_account("alice", "bob", "{}")

    wallet.api.transfer("alice", "bob", tt.Asset.Test(50), "lemon")

    wallet.api.transfer_to_vesting("alice", "bob", tt.Asset.Test(25))

    response = wallet.api.post_comment(
        "alice",
        "test-permlink",
        "",
        "xyz",
        "śćą",
        "DEBUG    test_tools.wallet.World.InitNodeWallet0:wallet.py:462 Closed with 0 return code",
        "{}",
    )

    _ops = response["operations"]
    assert _ops[0][0] == "comment"
    assert _ops[0][1]["author"] == "alice"
    assert _ops[0][1]["title"] == "u015bu0107u0105"
    assert (
        _ops[0][1]["body"] == "DEBUG    test_tools.wallet.World.InitNodeWallet0:wallet.py:462 Closed with 0 return code"
    )

    response = wallet.api.post_comment("alice", "test-permlink", "", "xyz", "TITLE NR 2", "BODY NR 2", "{}")

    _ops = response["operations"]

    assert _ops[0][0] == "comment"
    assert _ops[0][1]["author"] == "alice"
    assert _ops[0][1]["title"] == "TITLE NR 2"
    assert _ops[0][1]["body"] == "BODY NR 2"

    response = wallet.api.post_comment("bob", "bob-permlink", "alice", "test-permlink", "TITLE NR 3", "BODY NR 3", "{}")

    _ops = response["operations"]

    assert _ops[0][0] == "comment"
    assert _ops[0][1]["author"] == "bob"
    assert _ops[0][1]["parent_author"] == "alice"
    assert _ops[0][1]["title"] == "TITLE NR 3"
    assert _ops[0][1]["body"] == "BODY NR 3"

    response = wallet.api.vote("bob", "bob", "bob-permlink", 100)

    _ops = response["operations"]
    assert _ops[0][0] == "vote"
    assert _ops[0][1]["voter"] == "bob"
    assert _ops[0][1]["author"] == "bob"
    assert _ops[0][1]["permlink"] == "bob-permlink"
    assert _ops[0][1]["weight"] == 10000

    response = wallet.api.decline_voting_rights("alice", True)
    _ops = response["operations"]

    assert _ops[0][0] == "decline_voting_rights"
    assert _ops[0][1]["account"] == "alice"
