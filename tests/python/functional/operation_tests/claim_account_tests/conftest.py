from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def fee(request) -> tt.Asset.TestT:
    return request.param


@pytest.fixture()
def node(request, fee: tt.Asset.TestT) -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.config.plugin.append("account_history_api")
    init_node.config.log_logger =(
        '{"name": "default", "level": "debug", "appender": "stderr"}',
        '{"name": "user", "level": "debug", "appender": "stderr"}',
        '{"name": "chainlock", "level": "debug", "appender": "p2p"}',
        '{"name": "sync", "level": "debug", "appender": "p2p"}',
        '{"name": "p2p", "level": "debug", "appender": "p2p"}',
        '{"name":"witness","level":"debug","appender":"stderr"}'
    )

    init_node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=10),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[
                tt.HardforkSchedule(
                    block_num=1, hardfork=int(init_node.get_version()["version"]["blockchain_version"].split(".")[1])
                )
            ],
            init_supply=410_000_000_000,
            hbd_init_supply=30_000_000_000,
            initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=170_000_000_000),
        ),
    )

    wallet = tt.Wallet(attach_to=init_node)

    if fee > tt.Asset.Test(0):
        for witness in wallet.api.list_witnesses("", 100, only_names=True):
            wallet.api.update_witness(
                witness,
                "https://" + witness,
                tt.Account(witness).public_key,
                {"account_creation_fee": fee, "maximum_block_size": 65536, "hbd_interest_rate": 0},
            )

    init_node.wait_number_of_blocks(42)
    assert init_node.api.wallet_bridge.get_chain_properties().account_creation_fee == fee

    return init_node


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def alice(node: tt.InitNode, wallet: tt.Wallet) -> Account:
    wallet.create_account("alice")
    alice = Account("alice", node, wallet)
    alice.update_account_info()
    return alice


@pytest.fixture()
def wallet_alice(node: tt.InitNode) -> tt.Wallet:
    wallet = tt.Wallet(attach_to=node, preconfigure=False)
    wallet.api.import_key(tt.Account("alice").private_key)
    assert len(wallet.api.list_keys()) == 1, "Incorrect number of imported keys."
    return wallet


@pytest.fixture()
def initminer(node: tt.InitNode, wallet: tt.Wallet) -> Account:
    initminer = Account("initminer", node, wallet)
    initminer.update_account_info()
    return initminer


@pytest.fixture()
def initminer_wallet(node: tt.InitNode) -> tt.Wallet:
    wallet = tt.Wallet(attach_to=node)
    assert len(wallet.api.list_keys()) == 1, "Incorrect number of imported keys."
    return wallet
