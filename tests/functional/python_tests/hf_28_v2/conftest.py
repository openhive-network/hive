import pytest

import test_tools as tt

def run_with_faketime(node, time):
    #time example: '2020-01-01T00:00:00'
    requested_start_time = tt.Time.parse(time)
    node.run(time_offset=f'{tt.Time.serialize(requested_start_time, format_="@%Y-%m-%d %H:%M:%S")}')

@pytest.fixture
def node_hf27() -> tt.InitNode:
    node = tt.InitNode()
    run_with_faketime(node, '2023-02-07T10:10:10')
    return node


@pytest.fixture
def node_hf28() -> tt.InitNode:
    node = tt.InitNode()
    node.run()
    return node