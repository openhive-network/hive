from __future__ import annotations

import logging
import os
from pathlib import Path

import pytest

import test_tools as tt
from test_tools.__private.scope.scope_fixtures import *  # noqa: F403


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger("urllib3.connectionpool").propagate = False


@pytest.fixture()
def block_logs_for_testing_location() -> Path:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    return Path(destination_variable)


@pytest.fixture()
def block_log_empty_30_mono(block_logs_for_testing_location: Path) -> tt.BlockLog:
    block_logs_location: Path = block_logs_for_testing_location
    return tt.BlockLog(path=block_logs_location/ "empty_block_logs" / "empty_30", mode="monolithic")


@pytest.fixture()
def block_log_empty_430_split(block_logs_for_testing_location: Path) -> tt.BlockLog:
    block_logs_location: Path = block_logs_for_testing_location
    return tt.BlockLog(path=block_logs_location / "empty_block_logs" / "empty_430", mode="split")
