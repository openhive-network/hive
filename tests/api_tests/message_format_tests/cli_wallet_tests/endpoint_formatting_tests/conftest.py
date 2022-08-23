import pytest

import test_tools as tt

@pytest.fixture
def node():
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture
def wallet_with_text_formatter(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node, additional_arguments=['--output-formatter=text', '--transaction-serialization=legacy'])


@pytest.fixture
def wallet_with_json_formatter(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node, additional_arguments=['--output-formatter=json', '--transaction-serialization=legacy'])
