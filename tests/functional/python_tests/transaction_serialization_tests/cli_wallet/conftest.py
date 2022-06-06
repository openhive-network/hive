import pytest

import test_tools as tt


@pytest.fixture
def node():
    node = tt.InitNode()
    node.run()
    return node
