from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account

from .block_log.generate_block_log import WITNESSES


@pytest.fixture()
def node_with_custom_witnesses() -> tt.InitNode:
    block_log_directory = Path(__file__).parent / "block_log"

    node = tt.InitNode()
    node.config.log_logger =(
        '{"name": "default", "level": "debug", "appender": "stderr"}',
        '{"name": "user", "level": "debug", "appender": "stderr"}',
        '{"name": "chainlock", "level": "debug", "appender": "p2p"}',
        '{"name": "sync", "level": "debug", "appender": "p2p"}',
        '{"name": "p2p", "level": "debug", "appender": "p2p"}',
        '{"name":"witness","level":"debug","appender":"stderr"}'
    )
    node.config.block_log_split = -1

    for witness in WITNESSES:
        node.config.witness.append(witness)
        node.config.private_key.append(tt.Account(witness).private_key)

    node.run(replay_from=block_log_directory)

    return node


@pytest.fixture()
def wallet(node_with_custom_witnesses):
    return tt.Wallet(attach_to=node_with_custom_witnesses)


@pytest.fixture()
def voter(node_with_custom_witnesses, wallet):
    wallet.create_account("voter")
    return Account("voter", node_with_custom_witnesses, wallet)
