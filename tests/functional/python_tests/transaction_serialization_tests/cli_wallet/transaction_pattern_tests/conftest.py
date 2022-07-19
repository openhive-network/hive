from pathlib import Path
import pytest

import test_tools as tt


@pytest.fixture
def replayed_node():
    api_node = tt.ApiNode()
    api_node.run(replay_from=Path(__file__).parent / 'block_log' / 'block_log', wait_for_live=False)
    return api_node


@pytest.fixture
def wallet_with_type_of_serialization(replayed_node, request):
    wallet = tt.Wallet(attach_to=replayed_node,
                       additional_arguments=[f'--store-transaction={request.keywords.node.originalname}',
                                             f'--transaction-serialization={request.param}'])

    for account_name in ['alice', 'initminer', 'dan', 'bob']:
        wallet.api.import_key(tt.Account(account_name).private_key)

    # Return wallet, type_of_serialization and pattern_name
    return wallet, request.keywords.node.originalname
