from __future__ import annotations

from typing import List, NoReturn, Optional, Union, TYPE_CHECKING

import pytest
import test_tools as tt
from test_tools.__private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import

if TYPE_CHECKING:
    from _pytest.mark import Mark


def pytest_addoption(parser):
    parser.addoption("--http-endpoint", action="store", type=str, help="specifies http_endpoint of reference node")


@pytest.fixture
def node(request) -> Union[tt.InitNode, tt.RemoteNode]:
    """
    This fixture returns a node depending on the arguments passed to the @run_for decorator.
    Tests marking is described in the @run_for definition (located in `tests/local_tools.py`).
    * If your test is not decorated with @run_for, but this fixture is used, then `InitNode` is returned.
    * If you need to use in tests a node fixture other than this, just override node fixture, but @run_for won't work.

    Examples:
    1) Test is unmarked:
         def test_unmarked(node: tt.InitNode):
             node.api.some_api.some_method()

    2) Test is marked with @run_for decorator. User requested the `testnet` node:
         @run_for("testnet")
         def test_marked_with_run_for_decorator(node: tt.InitNode)
             node.api.some_api.some_method()

    3) Test is marked with @run_for decorator. User requested the `mainnet_5m` node:
         @run_for("mainnet_5m")
         def test_marked_with_run_for_decorator(node: tt.RemoteNode):
             node.api.some_api.some_method()

    4) Test is marked with @run_for decorator. User requested the `testnet` and `mainnet_5m` nodes:
         @run_for("testnet", "mainnet_5m")
         def test_marked_with_run_for_decorator(node: Union[tt.InitNode, tt.RemoteNode]):
             node.api.some_api.some_method()
    """
    def __create_init_node() -> tt.InitNode:
        init_node = tt.InitNode()
        init_node.run()
        return init_node

    def __create_remote_node() -> tt.RemoteNode:
        http_endpoint = request.config.getoption("--http-endpoint")
        if not http_endpoint:
            raise ValueError("Please specify the http_endpoint of remote node!")
        return tt.RemoteNode(http_endpoint=http_endpoint)

    def __get_all_supported_marks() -> list[Mark]:
        return list(filter(lambda mark: mark.name in create_node, all_marks))

    def __get_mark_set_by_run_for() -> Mark:
        return next(filter(lambda mark: mark.name == "parametrize" and mark.args[0] == "node", all_marks))

    def __is_test_marked_with_supported_nodes() -> bool:
        return any(filter(request.node.get_closest_marker, create_node))

    def __was_run_for_used() -> bool:
        return request.node.get_closest_marker("decorated_with_run_for") is not None

    def __assert_no_duplicated_mark_decorators_of_same_node() -> Optional[NoReturn]:
        all_supported_marks = __get_all_supported_marks()
        for mark in all_supported_marks:
            if all_supported_marks.count(mark) > 1:
                raise AssertionError(
                    f"Duplicated mark: `{mark.name}`.\n"
                    f"Must be specified only once in the `@run_for()` decorator.\n" + hint_message
                )

    def __assert_no_duplicated_node_in_run_for_params() -> Optional[NoReturn]:
        parametrize_mark = __get_mark_set_by_run_for()

        number_of_nodes_specified_in_run_for = len(parametrize_mark.args[1])
        number_of_unique_nodes_given_in_the_run_for_parameters = len(
            {mark_obj.values[0][0] for mark_obj in parametrize_mark.args[1]}
        )

        if number_of_nodes_specified_in_run_for != number_of_unique_nodes_given_in_the_run_for_parameters:
            raise AssertionError("Unallowed duplication of `@run_for()' decorator arguments.\n" + hint_message)

    def __assert_supported_nodes_used_only_in_run_for() -> Optional[NoReturn]:
        def __get_marks_created_by_run_for_only() -> List[Mark]:
            parametrize_mark = __get_mark_set_by_run_for()
            mark_decorators = sum([mark_decorator.marks for mark_decorator in parametrize_mark.args[1]], [])
            return [mark_decorator.mark for mark_decorator in mark_decorators]

        for mark in __get_all_supported_marks():
            if mark not in __get_marks_created_by_run_for_only():
                raise AssertionError(
                    f"Unallowed usage of a mark: `{mark}`.\n"
                    f"Please use the `@run_for()` decorator to mark test instead.\n" + hint_message
                )

    all_marks = request.node.own_markers

    create_node = {
        "testnet": __create_init_node,
        "mainnet_5m": __create_remote_node,
        "mainnet_64m": __create_remote_node,
    }

    hint_message = (
        f"You can use nodes: {list(create_node)}. Each value can be used only once.\n"
        f"To correctly mark your test, please use the following syntax:\n\n"
        f"""@run_for({", ".join(map(lambda node: f"'{node}'", create_node))})\n"""
        f"def {request.node.originalname}(node):\n"
        f"    <do something>"
    )

    __assert_no_duplicated_mark_decorators_of_same_node()
    if not __was_run_for_used():
        if not __is_test_marked_with_supported_nodes():
            return create_node["testnet"]()
        raise AssertionError(
            "Incorrect test marking. Use the `@run_for()` decorator to mark the test.\n" + hint_message
        )

    __assert_no_duplicated_node_in_run_for_params()
    __assert_supported_nodes_used_only_in_run_for()
    requested_node = __get_all_supported_marks()[0]
    return create_node[requested_node.name]()


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
