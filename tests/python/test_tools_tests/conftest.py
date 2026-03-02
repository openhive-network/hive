from __future__ import annotations

import logging
from typing import TYPE_CHECKING

import pytest
import test_tools as tt
from loguru import logger
from test_tools.__private.scope.scope_fixtures import *  # noqa: F403

from schemas.policies.policy import set_policies
from schemas.policies.testnet_assets import TestnetAssetsPolicy

if TYPE_CHECKING:
    from collections.abc import Iterable


@pytest.fixture(autouse=True)
def _disable_logging() -> None:
    logger.disable("helpy")


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger("urllib3.connectionpool").propagate = False
    tt.logger.enable("test_tools")


@pytest.fixture(autouse=True)
def _use_testnet_assets() -> None:
    set_policies(TestnetAssetsPolicy(use_testnet_assets=True))


@pytest.fixture(name="node")
def _node() -> tt.InitNode:  # noqa: PT005
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture
def two_networks_connected() -> Iterable[tt.Network]:
    first_network = tt.Network()
    tt.InitNode(network=first_network)

    second_network = tt.Network()
    tt.ApiNode(network=second_network)

    first_network.connect_with(second_network)

    return [first_network, second_network]


@pytest.fixture
def three_networks_connected(two_networks_connected: Iterable[tt.Network]) -> Iterable[tt.Network]:
    first_network, second_network = two_networks_connected

    third_network = tt.Network()
    tt.ApiNode(network=third_network)

    first_network.connect_with(third_network)

    return [first_network, second_network, third_network]


@pytest.fixture
def four_networks_connected(three_networks_connected: Iterable[tt.Network]) -> Iterable[tt.Network]:
    first_network, second_network, third_network = three_networks_connected

    fourth_network = tt.Network()
    tt.ApiNode(network=fourth_network)

    first_network.connect_with(fourth_network)

    return [first_network, second_network, third_network, fourth_network]
