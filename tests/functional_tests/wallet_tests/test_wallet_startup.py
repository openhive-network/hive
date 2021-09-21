import pytest

from test_tools import RemoteNode, Wallet
from test_tools.exceptions import NodeIsNotRunning


def test_attaching_wallet_to_local_node(world):
    node = world.create_init_node()
    node.run()

    Wallet(attach_to=node)


def test_attaching_wallet_to_remote_node(world):
    # Connecting with real remote node (e.g. from main net) is not reliable,
    # so wallet is connected to local node, but via remote node interface.
    local_node = world.create_init_node()
    local_node.run()

    remote_node = RemoteNode(local_node.get_http_endpoint(), ws_endpoint=local_node.get_ws_endpoint())

    Wallet(attach_to=remote_node)


def test_attaching_wallet_to_not_run_node(world):
    node = world.create_init_node()

    with pytest.raises(NodeIsNotRunning):
        Wallet(attach_to=node)
