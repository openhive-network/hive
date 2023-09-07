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
def receiver(node, wallet):
    wallet.create_account("receiver")
    receiver = RecurrentTransferAccount("receiver", node, wallet)
    receiver.update_account_info()
    return receiver


@pytest.fixture
def sender(request, node, wallet):
    params = request.node.callspec.params
    amount = [params[amount] for amount in params.keys() if "amount" in amount]
    execution = [params[execution] for execution in params.keys() if "execution" in execution]
    if len(amount) != len(execution):
        amount = [amount[0] for _ in range(len(execution))]
    top_up_sum = sum([amount * execution for amount, execution in zip(amount, execution)])

    wallet.create_account(
        name="sender",
        hives=top_up_sum if isinstance(top_up_sum, tt.Asset.Test) else None,
        hbds=top_up_sum if isinstance(top_up_sum, tt.Asset.Tbd) else None,
        vests=tt.Asset.Test(1)
    )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()
    return sender
