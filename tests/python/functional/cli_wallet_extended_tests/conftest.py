from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def node(request: pytest.FixtureRequest) -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.config.plugin.append("metadata")
    init_node.config.plugin.append("metadata_api")
    init_node.config.plugin.append("condenser_api")

    # Also respect enable_plugins marker from @run_for decorator
    mark = request.node.get_closest_marker("enable_plugins")
    if mark is not None:
        init_node.config.plugin.extend(mark.args[0])

    init_node.run(timeout=60.0, max_retries=3)
    return init_node


@pytest.fixture()
def wallet(node) -> tt.OldWallet:
    return tt.OldWallet(attach_to=node)
