from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation.withdrawe_vesting import PowerDownAccount


@pytest.fixture()
def prepared_node() -> tt.InitNode:
    witnesses = [f"witness-{num}" for num in range(20)]

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

    for witness in witnesses:
        node.config.witness.append(witness)
        node.config.private_key.append(tt.PrivateKey(witness))

    node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=0)],
            init_supply=410_000_000_000,
            hbd_init_supply=30_000_000_000,
            initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=170_000_000_000),
            init_witnesses=witnesses,
        ),
    )
    return node


@pytest.fixture()
def wallet(prepared_node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=prepared_node)


@pytest.fixture()
def alice(prepared_node, wallet):
    wallet.create_account("alice")
    alice = PowerDownAccount("alice", prepared_node, wallet)
    alice.fund_vests(tt.Asset.Test(13_000))
    alice.update_account_info()
    return alice
