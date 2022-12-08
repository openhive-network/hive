from typing import List

import pytest

from beem import Hive

from .local_tools import NodeClientMaker


def pytest_addoption(parser):
    parser.addoption("--chain-id", type=int, help="chain-id used for converting and running node")
    parser.addoption("--skeleton-key", type=str, help="skeleton-key used for converting and running node")


@pytest.fixture
def chain_id(request):
    return request.config.getoption("--chain-id") or 42


@pytest.fixture
def skeleton_key(request):
    return request.config.getoption("--skeleton-key") or "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"


@pytest.fixture
def node_client(node, worker_id) -> NodeClientMaker:
    def _node_client(accounts: List[dict] = None) -> Hive:
        accounts = accounts or []

        keys = node.config.private_key.copy()

        for account in accounts:
            keys.append(account["private_key"])

        node_url = f"http://{node.http_endpoint}"
        return Hive(node=node_url, no_broadcast=False, keys=keys, profile=worker_id, num_retries=-1)

    return _node_client
