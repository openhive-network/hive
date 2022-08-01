import pytest

import test_tools as tt


@pytest.fixture
def node():
    init_node = tt.InitNode()
    init_node.run()
    return init_node


# This node is created with of usage of faketimelib. Parameter time offset with these arguments means that node is
# created with actual date and time, but clock time is sped up 90 times. Faketimelib docs: https://github.com/wolfcw/libfaketime
@pytest.fixture
def sped_up_node():
    init_node = tt.InitNode()
    init_node.run(time_offset='+0 x90')
    return init_node


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)
