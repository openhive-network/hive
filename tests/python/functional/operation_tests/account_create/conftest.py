from __future__ import annotations

from datetime import datetime

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def alice(create_node_and_wallet: tuple[tt.InitNode, tt.Wallet]) -> Account:
    node, wallet = create_node_and_wallet
    wallet.create_account("alice", hives=200, hbds=200, vests=50)
    return Account("alice", node, wallet)


@pytest.fixture()
def alice_without_hive(create_node_and_wallet: tuple[tt.InitNode, tt.Wallet]) -> Account:
    node, wallet = create_node_and_wallet
    wallet.create_account("alice", hbds=200, vests=50)
    return Account("alice", node, wallet)


@pytest.fixture()
def account_creation_fee(request) -> tt.Asset.TestT:
    return request.param


@pytest.fixture()
def create_node_and_wallet(account_creation_fee: tt.Asset.TestT) -> tuple[tt.InitNode, tt.Wallet]:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    alternate_chain_spec = tt.AlternateChainSpecs(
        genesis_time=int(datetime.now().timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
        init_supply=410_000_000_000,
        hbd_init_supply=30_000_000_000,
        initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=170_000_000_000),
    )

    node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),
        alternate_chain_specs=alternate_chain_spec,
    )
    wallet = tt.Wallet(attach_to=node)
    wallet.api.update_witness(
        "initminer",
        "http://url.html",
        tt.Account("initminer").public_key,
        {"account_creation_fee": account_creation_fee},
    )

    node.wait_number_of_blocks(43)
    return node, wallet
