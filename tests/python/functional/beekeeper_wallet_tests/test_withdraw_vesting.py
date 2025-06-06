from __future__ import annotations

import test_tools as tt
from test_tools.__private.wallet.constants import WalletResponse

from .utilities import create_accounts


def test_withdraw_vesting(wallet: tt.Wallet) -> None:
    def check_withdraw_data(get_account: tt.Account, vesting_withdraw_rate: tt.Asset.Vest, to_withdraw: int) -> None:
        assert get_account["vesting_withdraw_rate"] == vesting_withdraw_rate
        assert get_account["to_withdraw"] == to_withdraw

    def check_route_data(withdraw_vesting_route: WalletResponse) -> dict:
        _ops = withdraw_vesting_route["operations"]

        assert _ops[0][0] == "set_withdraw_vesting_route_operation"

        return _ops[0][1]

    def check_route(check_route_data: dict, from_account: str, to_account: str, percent: int, auto_vest: bool) -> None:
        assert check_route_data["from_account"] == from_account
        assert check_route_data["to_account"] == to_account
        assert check_route_data["percent"] == percent
        assert check_route_data["auto_vest"] == auto_vest

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
