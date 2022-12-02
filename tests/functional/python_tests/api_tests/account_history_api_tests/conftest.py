import pytest

import test_tools as tt

@pytest.fixture
def ah_node() -> tt.InitNode:
    node = tt.InitNode()
    node.run()
    return node
