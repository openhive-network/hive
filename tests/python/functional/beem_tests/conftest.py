from __future__ import annotations

import copy
from typing import TYPE_CHECKING

import pytest
from beem import Hive

if TYPE_CHECKING:
    import test_tools as tt
    from hive_local_tools.functional.python.beem import NodeClientMaker


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption("--chain-id", type=int, help="chain-id used for converting and running node")
    parser.addoption("--skeleton-key", type=str, help="skeleton-key used for converting and running node")


@pytest.fixture()
def chain_id(request: pytest.FixtureRequest) -> str | int:
    return request.config.getoption("--chain-id") or 42


@pytest.fixture()
def skeleton_key(request: type[pytest.FixtureRequest]) -> str:
    return request.config.getoption("--skeleton-key") or "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"


@pytest.fixture()
def node_client(node: tt.InitNode | tt.RemoteNode, worker_id) -> NodeClientMaker:
    def _node_client(accounts: list[dict] | None = None) -> Hive:
        accounts = accounts or []

        keys = copy.deepcopy(node.config.private_key)

        for account in accounts:
            keys.append(account["private_key"])

        node_url = node.http_endpoint.as_string()
        return Hive(node=node_url, no_broadcast=False, keys=keys, profile=worker_id, num_retries=-1)

    return _node_client
