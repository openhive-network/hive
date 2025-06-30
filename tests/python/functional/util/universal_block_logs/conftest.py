from __future__ import annotations

import os
from pathlib import Path
from typing import Final, Literal

import pytest

import test_tools as tt
from python.functional.util.universal_block_logs.generate_universal_block_logs import (
    CHAIN_ID,
    SIGNERS,
    WITNESSES,
)

SHARED_MEMORY_FILE_SIZE: Final[int] = 24
WEBSERVER_THREAD_POOL_SIZE: Final[int] = 16


@pytest.fixture()
def test_id(request: pytest.FixtureRequest) -> str:
    return request.node.callspec.id


@pytest.fixture()
def replayed_node(request: pytest.FixtureRequest) -> tuple:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / request.param[0]
    block_log = tt.BlockLog(block_log_directory, "auto")
    acs = tt.AlternateChainSpecs.parse_file(block_log_directory / tt.AlternateChainSpecs.FILENAME)

    node = tt.InitNode()

    # remove unused plugins, to speed up node replay
    node.config.plugin.remove("account_by_key")
    node.config.plugin.remove("state_snapshot")
    node.config.plugin.remove("account_by_key_api")
    node.config.shared_file_size = f"{SHARED_MEMORY_FILE_SIZE}G"
    node.config.webserver_thread_pool_size = f"{WEBSERVER_THREAD_POOL_SIZE!s}"
    node.config.log_logger = (
        '{"name":"default","level":"info","appender":"stderr"} '
        '{"name":"user","level":"debug","appender":"stderr"} '
        '{"name":"chainlock","level":"error","appender":"p2p"} '
        '{"name":"sync","level":"debug","appender":"p2p"} '
        '{"name":"p2p","level":"debug","appender":"p2p"}'
    )
    node.config.block_log_split = 9999

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log,
        time_control=tt.StartTimeControl(start_time="head_block_time"),
        timeout=240,
        wait_for_live=True,
        alternate_chain_specs=acs,
        arguments=[f"--chain-id={CHAIN_ID}"],
    )

    wallet = tt.OldWallet(attach_to=node, additional_arguments=[f"--chain-id={CHAIN_ID}"])
    wallet.api.set_transaction_expiration(seconds=60)
    import_keys(wallet, request.param[0])

    return node, wallet, request.param[1]


def import_keys(
    wallet: tt.OldWallet,
    block_log_type: Literal[
        "multi_sign_universal_block_log",
        "open_sign_universal_block_log",
        "single_sign_universal_block_log",
        "maximum_sign_universal_block_log",
    ],
) -> None:
    if "multi_sign" in block_log_type:
        keys = [tt.PrivateKey("account", secret=f"secret-{num}") for num in range(5)]
        wallet.api.import_keys(keys)
    elif "single_sign" in block_log_type or "open_sign" in block_log_type:
        wallet.api.import_key(tt.PrivateKey("account", secret="secret"))
    elif "maximum_sign" in block_log_type:
        for signer in SIGNERS:
            keys = [tt.PrivateKey(signer, secret=f"secret-{num}") for num in range(40)]
            wallet.api.import_keys(keys)
    else:
        raise ValueError(f"Unsupported block_log_type: {block_log_type}")
