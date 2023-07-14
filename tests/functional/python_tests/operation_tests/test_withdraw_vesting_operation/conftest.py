import pytest

import test_tools as tt


@pytest.fixture
def node():
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.run(time_offset="+0h x5")
    # On the testnet, the OBI (one block irreversibility) mechanism starts after the 30th block. The OBI is necessary
    # to change the node's time offset without waiting 21 blocks until they become irreversible.
    node.wait_for_block_with_number(30)
    return node


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)
