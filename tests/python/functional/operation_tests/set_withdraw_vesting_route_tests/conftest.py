from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation.withdrawe_vesting import PowerDownAccount

if TYPE_CHECKING:
    from hive_local_tools.functional.python.operation.set_withdraw_vesting_route_tests import VestingRouteParameters


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.log_logger = (
        '{"name": "default", "level": "debug", "appender": "stderr"}',
        '{"name": "user", "level": "debug", "appender": "stderr"}',
        '{"name": "chainlock", "level": "debug", "appender": "p2p"}',
        '{"name": "sync", "level": "debug", "appender": "p2p"}',
        '{"name": "p2p", "level": "debug", "appender": "p2p"}',
        '{"name":"witness","level":"debug","appender":"stderr"}'
    )
    node.run(
        time_control=tt.StartTimeControl(speed_up_rate=5),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            init_supply=20_000_000_000,
            hbd_init_supply=100_000_000,
            initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=10_000_000_000),
            hardfork_schedule=[
                tt.HardforkSchedule(
                    hardfork=int(node.get_version()["version"]["blockchain_version"].split(".")[1]), block_num=1
                )
            ],
        ),
    )
    return node


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def create_receivers(node: tt.InitNode, wallet: tt.Wallet) -> list[PowerDownAccount]:
    def create(parameters: list[VestingRouteParameters]) -> list[PowerDownAccount]:
        users = []
        accounts = wallet.list_accounts()
        for user in parameters:
            if user.receiver not in accounts and user.receiver not in [u.name for u in users]:
                wallet.create_account(user.receiver)
                users.append(PowerDownAccount(user.receiver, node, wallet))
                users[-1].update_account_info()
        return users

    return create


@pytest.fixture()
def user_a(node: tt.InitNode, wallet: tt.Wallet) -> PowerDownAccount:
    wallet.create_account("user-a")
    user_a = PowerDownAccount("user-a", node, wallet)
    user_a.fund_vests(tt.Asset.Test(13_000))
    user_a.update_account_info()
    return user_a
