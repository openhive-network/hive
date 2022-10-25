from pathlib import Path
from typing import List, Optional, Union

import pytest

from test_tools.__private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import
import test_tools as tt


def pytest_addoption(parser):
    parser.addoption("--http-endpoint", action="store", type=str, help='specifies http_endpoint of reference node')
    parser.addoption("--ws-endpoint", action="store", type=str, help='specifies ws_endpoint of reference node')
    parser.addoption("--wallet-path", action="store", type=str, help='specifies path of reference cli wallet')


@pytest.fixture
def node(request) -> Union[tt.InitNode, tt.RemoteNode]:
    def __create_init_node() -> tt.InitNode:
        init_node = tt.InitNode()
        init_node.run()
        return init_node

    def __create_replayed_node() -> tt.ApiNode:
        api_node = tt.ApiNode()
        api_node.run(replay_from=Path(__file__).parent.joinpath('api_tests/message_format_tests/wallet_bridge_api_tests/block_log/block_log'), wait_for_live=False)
        return api_node

    def __assert_no_duplicated_requests_of_same_node() -> None:
        requested_markers: List[str] = [marker.name for marker in request.node.iter_markers() if marker.name != 'parametrize']
        # jeżeli ilość marków żądanych jest różna z ilością żądanych marków unikatowych
        if len(requested_markers) != len(set(requested_markers)):
            raise RuntimeError(f'Duplicated marker. You used two or more times markers: {requested_markers}. Do not use duplicates.')

    def __assert_no_duplicated_node_in_run_for_params() -> None:
        for marker in request.node.iter_markers():
            if marker.name == 'parametrize' and marker.args[0] == 'node':
                number_of_nodes_specified_in_run_for = len(marker.args[1])
                number_of_unique_nodes_given_in_the_run_for_parameters = len(set([marker_obj.values[0][0] for marker_obj in marker.args[1]]))

                if number_of_nodes_specified_in_run_for != number_of_unique_nodes_given_in_the_run_for_parameters:
                    raise RuntimeError(f"Duplicate requested `nodes` in `@run_for' function parameters."
                                       f" Use only defined nodes: {', '.join(create_node.keys())}.")

    def __get_requested_node() -> Optional[str]:
        requested_nodes: List[str] = [request.node.get_closest_marker(node).name for node in create_node if
                                      request.node.get_closest_marker(node) is not None]

        if len(requested_nodes) > 1:
            raise RuntimeError(f'Incorrect test marking. You cannot mark mixing function `@run_for` with classic mark.\n'
                               f'To the mark multiple node test, use decorator:\n'
                               f'@run_for({", ".join(requested_nodes)})\n'
                               f'def {request.node.originalname}(node):\n'
                               f'    <do something>')

        return requested_nodes[0] if len(requested_nodes) == 1 else None

    def __run_for_was_used() -> bool:
        marker_names = [marker for marker in request.node.iter_markers()]
        for marker in marker_names:
            if marker.name == 'parametrize' and marker.args[0] == 'node':
                for mark_name in marker.args[1]:
                    if marker_names.count(mark_name[1][0].mark) == 1:
                        return True
        return False

    def __mark_exist(name: str) -> bool:
        mark = request.node.get_closest_marker(name)
        if hasattr(mark, "name"):
            if mark.name == name:
                return True
        return False

    create_node = {
        'testnet': __create_init_node if not __mark_exist('replay_prepared_block_log') else __create_replayed_node,
        'mainnet_5m': lambda: tt.RemoteNode(http_endpoint=request.config.getoption("--http-endpoint")),
        'mainnet_64m': lambda: tt.RemoteNode(http_endpoint=request.config.getoption("--http-endpoint")),
    }

    __assert_no_duplicated_requests_of_same_node()
    # markery przypisane do testu zgadzające się ze zdefiniowanymi nodami w [create_node]
    requested_nodes: List[str] = [request.node.get_closest_marker(node).name for node in create_node
                                  if request.node.get_closest_marker(node) is not None]

    if __run_for_was_used():
        __assert_no_duplicated_node_in_run_for_params()
        requested_node = __get_requested_node()
        return create_node[requested_node]()

    if requested_nodes:
        raise RuntimeError(
            f"Incorrect test marking. Use the `@run_for` function to mark the test. Available nodes: {', '.join(create_node.keys())}.\n"
            f"To mark correctly your test use syntax:\n"
            f"@run_for({', '.join(requested_nodes)})\n"
            f"def {request.node.originalname}(node):\n"
            f"    <do something>"
        )
    if list(request.node.iter_markers()) == [] or request.node.get_closest_marker('parametrize') is not None:
        return create_node['testnet']()


@pytest.fixture
def should_prepare(node) -> bool:
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
    def test_some_method(node, should_prepare):
        if should_prepare:
            perform_additional_preparation(node)  # Optional preparation only executed in testnet

        node.api.some_api.some_method()  # Common part
    ```
    """
    return not isinstance(node, tt.RemoteNode)
