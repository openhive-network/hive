from __future__ import annotations

from pathlib import Path

import pytest

import shared_tools.networks_architecture as networks
import test_tools as tt
from shared_tools.complex_networks import prepare_network

from .block_log.generate_block_log import CONFIG


@pytest.fixture
def prepare_environment() -> networks.NetworksBuilder:
    block_log_directory = Path(__file__).parent / "block_log"

    architecture = networks.NetworksArchitecture()
    architecture.load(CONFIG)
    tt.logger.info(architecture)

    yield prepare_network(architecture, block_log_directory)
