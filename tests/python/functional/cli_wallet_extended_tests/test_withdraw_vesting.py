import test_tools as tt

from .utilities import create_accounts


def test_withdraw_vesting(wallet):
    def check_withdraw_data(node, vesting_withdraw_rate, to_withdraw):
        assert node["vesting_withdraw_rate"] == vesting_withdraw_rate
        assert node["to_withdraw"] == to_withdraw

    def check_route_data(node):
        _ops = node["operations"]

        assert _ops[0][0] == "set_withdraw_vesting_route"

        return _ops[0][1]

    def check_route(node, from_account, to_account, percent, auto_vest):
        assert node["from_account"] == from_account
        assert node["to_account"] == to_account
        assert node["percent"] == percent
        assert node["auto_vest"] == auto_vest

    create_accounts(wallet, "initminer", ["alice", "bob", "carol"])

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(500))

    response = wallet.api.get_account("alice")

    check_withdraw_data(response, tt.Asset.Vest(0), 0)

    assert len(wallet.api.get_withdraw_routes("alice", "incoming")) == 0

    wallet.api.withdraw_vesting("alice", tt.Asset.Vest(4))

    response = wallet.api.get_account("alice")

    check_withdraw_data(response, tt.Asset.Vest(0.307693), 4000000)

    assert len(wallet.api.get_withdraw_routes("alice", "incoming")) == 0

    response = wallet.api.set_withdraw_vesting_route("alice", "bob", 30, True)

    _value = check_route_data(response)
    check_route(_value, "alice", "bob", 30, True)

    _result = wallet.api.get_withdraw_routes("alice", "outgoing")
    assert len(_result) == 1
    check_route(_result[0], "alice", "bob", 30, True)

    response = wallet.api.set_withdraw_vesting_route("alice", "carol", 25, False)

    _value = check_route_data(response)
    check_route(_value, "alice", "carol", 25, False)

    _result = wallet.api.get_withdraw_routes("alice", "outgoing")
    assert len(_result) == 2

    check_route(_result[0], "alice", "bob", 30, True)
    check_route(_result[1], "alice", "carol", 25, False)
