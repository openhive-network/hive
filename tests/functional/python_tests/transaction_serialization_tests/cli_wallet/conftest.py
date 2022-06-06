import pytest

import test_tools as tt


@pytest.fixture
def node():
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture
def prepared_wallet(node, request):
    if request.param == 'modern':
        yield tt.Wallet(attach_to=node, additional_arguments=['--transaction-serialization=hf26'])
    elif request.param == 'legacy':
        yield tt.Wallet(attach_to=node, additional_arguments=['--transaction-serialization=legacy'])
    else:
        raise RuntimeError(f'Unsupported argument value "{request.param}", should be "modern" or "legacy"')
