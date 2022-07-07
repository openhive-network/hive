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


@pytest.fixture
def wallet_with_legacy_serialization(node, request):
    return tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                           '--transaction-serialization=legacy'])

@pytest.fixture
def wallet_with_nai_serialization(node, request):
    return tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                           '--transaction-serialization=hf26'])
