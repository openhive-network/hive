import pytest

from test_tools.__private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import
import test_tools as tt


def pytest_addoption(parser):
    parser.addoption("--http-endpoint", action="store", type=str, help='specifies http_endpoint of reference node')


@pytest.fixture
def prepared_node(request):
    def __create_init_node():
        init_node = tt.InitNode()
        init_node.run()
        return init_node

    create_node = {
        'testnet': __create_init_node,
        'mainnet_5m': lambda: tt.RemoteNode(http_endpoint=request.config.getoption("--http-endpoint")),
        'mainnet_64m': lambda: tt.RemoteNode(http_endpoint=request.config.getoption("--http-endpoint")),
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
    """
    When tests are run on mainnet or mirrornet node, where block log contains operations and blockchain state reflects
    real world blockchain, there is no need to perform any additional preparation for most of test cases. Tests usually
    just send request to node and validate response.

    However, when tests are run in testnet, there are no operations in blockchain. Tests each time work on new, empty
    network. So before each test, blockchain has to be prepared, to make validation possible.

    This is a purpose of `should_prepare` flag. It is set for networks, which are not ready for tests and need to be
    prepared before. It allows to reuse one test for different nodes, because test can include optional preparation.
    It is useful when `run_for` decorator is used.

    Example use case:
    ```
    @run_for('testnet', 'mainnet_5m')
    def test_some_method(prepared_node, should_prepare):
        if should_prepare:
            perform_additional_preparation(prepared_node)  # Optional preparation only executed in testnet

        prepared_node.api.some_api.some_method()  # Common part
    ```
    """
    return not isinstance(prepared_node, tt.RemoteNode)
