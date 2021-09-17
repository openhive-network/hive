import pytest

from test_tools import Wallet
from test_tools.exceptions import NodeIsNotRunning


def test_attaching_wallet_to_node(world):
    node = world.create_init_node()
    node.run()

    Wallet(attach_to=node)


def test_attaching_wallet_to_not_run_node(world):
    node = world.create_init_node()

    with pytest.raises(NodeIsNotRunning):
        Wallet(attach_to=node)
