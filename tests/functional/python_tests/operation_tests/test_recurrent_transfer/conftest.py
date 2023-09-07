import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation.recurrent_transfer import RecurrentTransferAccount


@pytest.fixture
def node(speed_up_node) -> tt.InitNode:
    speed_up_node.set_vest_price(quote=tt.Asset.Vest(1800))
    speed_up_node.wait_for_block_with_number(30)
    return speed_up_node


@pytest.fixture
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture
def receiver(request, node, wallet):
    wallet.create_account("receiver")
    receiver = RecurrentTransferAccount("receiver", node, wallet)
    receiver._update_account_info()
    return receiver
