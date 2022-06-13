import pytest
import logging

import test_tools as tt

from test_tools.__private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger('urllib3.connectionpool').propagate = False


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)


@pytest.fixture
def prepared_node(request):
    def __create_init_node():
        init_node = tt.InitNode()
        init_node.run()
        return init_node

    create_node = {
        'testnet': __create_init_node,
        'mainnet_5m': lambda: tt.RemoteNode(http_endpoint='http://192.168.6.129:18091'),
        'mainnet_64m': lambda: tt.RemoteNode(http_endpoint='http://192.168.6.129:18091'),
    }

    requested_node = request.param[0]
    try:
        yield create_node[requested_node]()
    except KeyError as exception:
        supported_nodes = ', '.join(f'"{name}"' for name in create_node.keys())
        raise RuntimeError(
            f'Requested node name "{requested_node}" is not supported. '
            f'Use one of following: {supported_nodes}.'
        ) from exception


@pytest.fixture
def should_prepare(prepared_node) -> bool:
    """TODO: Doc"""
    return not isinstance(prepared_node, tt.RemoteNode)
