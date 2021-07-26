import pytest

from test_tools.exceptions import NodeIsNotRunning


def test_attaching_wallet_to_node(world):
    node = world.create_init_node()
    node.run()

    wallet = node.attach_wallet()
    assert wallet.is_running()


def test_attaching_wallet_to_not_run_node(world):
    node = world.create_init_node()

    with pytest.raises(NodeIsNotRunning):
        node.attach_wallet()
