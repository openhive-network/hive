from __future__ import annotations

import os
from pathlib import Path

import pytest

import shared_tools.networks_architecture as networks
import test_tools as tt
from shared_tools.complex_networks import prepare_network

from .block_log.generate_block_log import CONFIG


@pytest.fixture()
def prepare_environment() -> networks.NetworksBuilder:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / "comment_cashout" / "comments_and_votes"

    architecture = networks.NetworksArchitecture()
    architecture.load(CONFIG)
    tt.logger.info(architecture)

    return prepare_network(architecture, block_log_directory)
