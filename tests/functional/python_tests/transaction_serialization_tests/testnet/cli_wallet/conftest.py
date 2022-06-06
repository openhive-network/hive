import pytest

from test_tools import Wallet, World


@pytest.fixture
def node(world: World):
    node = world.create_init_node()
    node.run()
    return node


@pytest.fixture
def prepared_wallet(node, request):
    if request.param == 'modern':
        yield Wallet(attach_to=node, additional_arguments=['--transaction-serialization=hf26'])
    elif request.param == 'legacy':
        yield Wallet(attach_to=node, additional_arguments=['--transaction-serialization=legacy'])
    else:
        raise RuntimeError(f'Unsupported argument value "{request.param}", should be "modern" or "legacy"')
