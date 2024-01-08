from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import ALTERNATE_CHAIN_JSON_FILENAME, create_alternate_chain_spec_file
from hive_local_tools.functional.python.operation.withdrawe_vesting import PowerDownAccount


@pytest.fixture()
def prepared_node() -> tt.InitNode:
    witnesses = [f"witness-{num}" for num in range(20)]

    create_alternate_chain_spec_file(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[{"hardfork": 28, "block_num": 0}],
        init_supply=410_000_000_000,
        hbd_init_supply=30_000_000_000,
        initial_vesting={"vests_per_hive": 1800, "hive_amount": 170_000_000_000},
        init_witnesses=witnesses,
    )

    node = tt.InitNode()
    node.config.plugin.append("account_history_api")

    for witness in witnesses:
        node.config.witness.append(witness)
        node.config.private_key.append(tt.PrivateKey(witness))

    node.run(
        time_offset="+0h x5",
        arguments=["--alternate-chain-spec", str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)],
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
