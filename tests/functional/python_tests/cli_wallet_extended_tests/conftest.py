import pytest

import test_tools as tt


@pytest.fixture
def node() -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.run()
    return init_node


@pytest.fixture
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)
