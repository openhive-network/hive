import pytest

from test_tools import Wallet
from test_tools.exceptions import CommunicationError


@pytest.fixture
def wallet(world):
    init_node = world.create_init_node()
    init_node.run()

    return Wallet(attach_to=init_node)


def test_if_raise_when_parameters_are_bad(wallet):
    with pytest.raises(CommunicationError):
        wallet.api.create_account('surely', 'bad', 'arguments')


def test_if_raise_when_operation_is_invalid(wallet):
    with pytest.raises(CommunicationError):
        # Operation is invalid because account "alice" doesn't exists
        wallet.api.transfer('initminer', 'alice', '1.000 TESTS', 'memo')
