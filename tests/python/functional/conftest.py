from __future__ import annotations

import contextlib
import logging
import os
from pathlib import Path

import pytest

import test_tools as tt
from test_tools.__private.scope.scope_fixtures import *  # noqa: F403


def pytest_configure(config) -> None:
    """
    Warm up msgspec decoders early in each pytest-xdist worker process.

    msgspec's decoder initialization involves Python's typing module internals
    which are not thread-safe. When tests run in parallel (via pytest-xdist),
    each worker process needs its own warmup to avoid segfaults during
    concurrent API calls that trigger decoder initialization.
    """
    with contextlib.suppress(Exception):
        # Import ALL API modules to trigger annotation resolution
        # This forces all typing annotations to be resolved in the main thread
        # before any xdist workers start making concurrent API calls
        import hiveio_api.account_by_key_api
        import hiveio_api.account_history_api
        import hiveio_api.block_api
        import hiveio_api.condenser_api
        import hiveio_api.database_api
        import hiveio_api.debug_node_api
        import hiveio_api.follow_api
        import hiveio_api.jsonrpc
        import hiveio_api.market_history_api
        import hiveio_api.network_broadcast_api
        import hiveio_api.rc_api
        import hiveio_api.reputation_api  # noqa: F401

        import schemas.apis.app_status_api  # Used by beekeepy.__discover_ports()
        import schemas.apis.network_node_api
        import schemas.apis.test_api
        import schemas.apis.wallet_bridge_api
        import schemas.fields.basic
        import schemas.fields.hex
        import schemas.operations
        import schemas.transaction  # noqa: F401
        from schemas import decoders
        from schemas.jsonrpc import get_response_model

        # Pre-build all decoders to avoid lazy initialization during tests
        for hf in ["hf26"]:
            decoders.get_hf26_decoder.cache_clear()  # Clear any existing cache
            get_response_model(str, '{"jsonrpc":"2.0","id":0,"result":"warmup"}', hf)
            get_response_model(dict, '{"jsonrpc":"2.0","id":0,"result":{}}', hf)
            get_response_model(list, '{"jsonrpc":"2.0","id":0,"result":[]}', hf)
            get_response_model(int, '{"jsonrpc":"2.0","id":0,"result":0}', hf)
            get_response_model(bool, '{"jsonrpc":"2.0","id":0,"result":true}', hf)


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger("urllib3.connectionpool").propagate = False


@pytest.fixture
def block_logs_for_testing_location() -> Path:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    return Path(destination_variable)


@pytest.fixture
def block_log_empty_30_mono(block_logs_for_testing_location: Path) -> tt.BlockLog:
    block_logs_location: Path = block_logs_for_testing_location
    return tt.BlockLog(path=block_logs_location / "empty_block_logs" / "empty_30", mode="monolithic")


@pytest.fixture
def block_log_empty_430_split(block_logs_for_testing_location: Path) -> tt.BlockLog:
    block_logs_location: Path = block_logs_for_testing_location
    return tt.BlockLog(path=block_logs_location / "empty_block_logs" / "empty_430", mode="split")
