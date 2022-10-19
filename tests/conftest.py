from typing import List, Literal, Union

import pytest

from test_tools.__private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import
import test_tools as tt


def pytest_addoption(parser):
    parser.addoption("--http-endpoint", action="store", type=str, help='specifies http_endpoint of reference node')


@pytest.fixture
def prepared_node(request) -> Union[tt.InitNode, tt.RemoteNode]:
    def __create_init_node() -> tt.InitNode:
        init_node = tt.InitNode()
        init_node.run()
        return init_node

    def __get_requested_node_markers() -> List[Literal['testnet', 'mainnet_5m', 'mainnet_64m']]:
        requested_node_markers = []
        for marker in request.node.iter_markers():
            if marker.name in create_node:
                requested_node_markers.append(marker.name)
        return requested_node_markers

    def __assert_no_duplicated_requests_of_same_node() -> None:
        requested_markers = __get_requested_node_markers()
        for marker in requested_markers:
            if requested_markers.count(marker) > 1:
                raise AssertionError(f'Duplicated marker "{marker}". Must be used only once.')

    def __assert_no_duplicated_node_in_run_for_params() -> None:
        for marker in request.node.iter_markers():
            if marker.name == 'parametrize' and marker.args[0] == 'node':
                number_of_nodes_specified_in_run_for = len(marker.args[1])
                number_of_unique_nodes_given_in_the_run_for_parameters = len(set([marker_obj.values[0][0] for marker_obj in marker.args[1]]))

                if number_of_nodes_specified_in_run_for != number_of_unique_nodes_given_in_the_run_for_parameters:
                    raise AssertionError(f"Duplicate requested `nodes` in `@run_for' function parameters."
                                         f" Use only defined nodes: {', '.join(create_node.keys())}.")

    def __was_run_for_used() -> bool:
        return request.node.get_closest_marker('decorated_with_run_for') is not None

    def __is_run_for_node() -> bool:
        requested_nodes: List[str] = [request.node.get_closest_marker(node).name for node in create_node
                                      if request.node.get_closest_marker(node) is not None]
        return True if not requested_nodes == [] else False

    def __check_the_correctness_of_the_marks_used_outside_the_function_run_for() -> None:
        # wszystkie markery którymi test jest oznaczony
        markers = request.node.own_markers

        # paremetrize dodany automatycznie funkcją run_for
        parametrize_marker = next(filter(lambda marker: marker.name == 'parametrize' and marker.args[0] == 'node', markers))

        # wewnętrzne markery z parametriza dodanego run_forem (mark_decoratory)
        mark_decorators = sum([mark_decorator.marks for mark_decorator in parametrize_marker.args[1]], [])

        # wewnętrzne markery z parametriza (same marki)
        inside_run_for_param = [mark_decorator.mark for mark_decorator in mark_decorators]

        # zewnętrzne markery podane do testu zgodne z `dopuszczonymi` nodami
        required_markers = list(filter(lambda marker: marker.name in create_node, markers))

        # sprawdzenie markerów zewnętrznych z tymi które są podane w (run_for) parametrize
        for mark in required_markers:
            if mark not in inside_run_for_param:
                raise AssertionError(f'Incorrect use marker: "{mark.name}". Use the run_for function to mark test.')

    create_node = {
        'testnet': __create_init_node,
        'mainnet_5m': lambda: tt.RemoteNode(http_endpoint=request.config.getoption("--http-endpoint")),
        'mainnet_64m': lambda: tt.RemoteNode(http_endpoint=request.config.getoption("--http-endpoint")),
    }

    __assert_no_duplicated_requests_of_same_node()
    if not __was_run_for_used():
        if not __is_run_for_node():
            return create_node['testnet']()
        raise AssertionError(
            f"Incorrect test marking. Use the `@run_for` function to mark the test. Available nodes: {', '.join(create_node.keys())}.\n"
            f"To mark correctly your test use syntax:\n"
            f"@run_for({list(create_node.keys())})\n"
            f"def {request.node.originalname}(node):\n"
            f"    <do something>"
        )
    else:
        __assert_no_duplicated_node_in_run_for_params()
        __check_the_correctness_of_the_marks_used_outside_the_function_run_for()
        requested_node = __get_requested_node_markers()[0]
        return create_node[requested_node]()


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
