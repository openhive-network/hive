from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from tests.functional.full_block_generator.full_block_generator import (
    BLOCK_LOG_DIRECTORY,
    CHAIN_ID,
    SHARED_MEMORY_FILE_DIRECTORY,
    SHARED_MEMORY_FILE_SIZE,
    WEBSERVER_THREAD_POOL_SIZE,
    WITNESSES,
)


@pytest.fixture()
def replayed_node(request: pytest.FixtureRequest) -> tt.InitNode:
    block_log = tt.BlockLog(BLOCK_LOG_DIRECTORY / "block_log")
    alternate_chain_spec_path = BLOCK_LOG_DIRECTORY / tt.AlternateChainSpecs.FILENAME

    node = tt.InitNode()

    node.config.plugin.remove("account_by_key")
    node.config.plugin.remove("state_snapshot")
    node.config.plugin.remove("account_by_key_api")
    node.config.shared_file_size = f"{SHARED_MEMORY_FILE_SIZE}G"
    node.config.webserver_thread_pool_size = f"{str(WEBSERVER_THREAD_POOL_SIZE)}"
    node.config.log_logger = (
        '{"name":"default","level":"info","appender":"stderr"} '
        '{"name":"user","level":"debug","appender":"stderr"} '
        '{"name":"chainlock","level":"error","appender":"p2p"} '
        '{"name":"sync","level":"debug","appender":"p2p"} '
        '{"name":"p2p","level":"debug","appender":"p2p"}'
    )

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log,
        time_control=tt.Time.serialize(block_log.get_head_block_time(), format_=tt.TimeFormats.FAKETIME_FORMAT),
        timeout=120,
        wait_for_live=True,
        alternate_chain_specs=tt.AlternateChainSpecs.parse_file(alternate_chain_spec_path),
        arguments=[f"--shared-file-dir={SHARED_MEMORY_FILE_DIRECTORY}", f"--chain-id={CHAIN_ID}"],
    )

    wallet = tt.Wallet(attach_to=node, additional_arguments=[f"--chain-id={CHAIN_ID}"])
    import_keys(wallet, request.param[0])

    return node, wallet, request.param[1]


def import_keys(wallet: tt.Wallet, block_log_type: str) -> None:
    match block_log_type:
        case "block_log_multi_sign":
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"owner-{num}") for num in range(3)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"active-{num}") for num in range(6)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"posting-{num}") for num in range(10)])
        case "block_log_single_sign":
            wallet.api.import_key(tt.PrivateKey("account", secret="owner"))
            wallet.api.import_key(tt.PrivateKey("account", secret="active"))
            wallet.api.import_key(tt.PrivateKey("account", secret="posting"))
