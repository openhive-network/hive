from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME
from hive_local_tools.functional.python.operation import Account, Proposal

from .block_log.generate_block_log import WITNESSES


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")

    for witness in WITNESSES:
        node.config.witness.append(witness)
        node.config.private_key.append(tt.Account(witness).private_key)

    block_log_directory = Path(__file__).parent.joinpath("block_log")

    with open(block_log_directory / "timestamp", encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    node.run(
        time_offset=f"{tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT)}",
        replay_from=block_log_directory / "block_log",
        arguments=["--alternate-chain-spec", str(block_log_directory / ALTERNATE_CHAIN_JSON_FILENAME)],
    )
    return node


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def alice(node: tt.InitNode, wallet: tt.Wallet) -> Account:
    wallet.create_account("alice", vests=tt.Asset.Test(20))
    return Account("alice", node, wallet)


@pytest.fixture()
def bob(node: tt.InitNode, wallet: tt.Wallet) -> Account:
    wallet.create_account("bob", vests=tt.Asset.Test(10))
    bob = Account("bob", node, wallet)
    bob.update_account_info()
    bob.unlock_delayed_votes()
    return bob


@pytest.fixture()
def proposal_x(node: tt.InitNode) -> Proposal:
    return Proposal(node, proposal_id=0)


@pytest.fixture()
def proposal_y(node: tt.InitNode) -> Proposal:
    return Proposal(node, proposal_id=1)


@pytest.fixture()
def proposal_z(node: tt.InitNode) -> Proposal:
    return Proposal(node, proposal_id=2)
