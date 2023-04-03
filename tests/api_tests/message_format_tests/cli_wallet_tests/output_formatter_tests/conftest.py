from pathlib import Path

import pytest

import test_tools as tt

@pytest.fixture
def node(request):
    if request.node.get_closest_marker('replayed_node') is None:
        node = tt.InitNode()
        node.run()
        return node
    api_node = tt.FullApiNode()
    api_node.run(replay_from=Path(__file__).parent.joinpath('block_log/block_log'), wait_for_live=False)
    return api_node


@pytest.fixture
def wallet_with_text_formatter(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node, additional_arguments=['--output-formatter=text', '--transaction-serialization=legacy'])


@pytest.fixture
def wallet_with_json_formatter(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node, additional_arguments=['--output-formatter=json', '--transaction-serialization=legacy'])
