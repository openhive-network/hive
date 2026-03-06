from __future__ import annotations

import os
from pathlib import Path

import pytest

import test_tools as tt
from test_tools import complex_networks as ttcn

from .block_log.generate_block_log import CONFIG


@pytest.fixture()
def prepare_environment() -> ttcn.NetworksBuilder:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / "comment_cashout" / "comments_and_votes"

    architecture = ttcn.NetworksArchitecture()
    architecture.load(CONFIG)
    tt.logger.info(architecture)

    return ttcn.prepare_network(architecture, block_log_directory)
