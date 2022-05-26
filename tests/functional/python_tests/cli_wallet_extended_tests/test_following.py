import json

from test_tools import Asset
from .utilities import create_accounts


def test_following(wallet):
    create_accounts( wallet, 'initminer', ['alice', 'bob'] )

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))

    response = wallet.api.follow('alice', 'bob', ['blog'])
    operation = response['operations'][0]
    operation_type, operation_json = operation[0], json.loads(operation[1]['json'])

    assert operation_type == 'custom_json'
    assert operation_json == {
        'type': 'follow_operation',
        'value': {
            'follower': 'alice',
            'following': '@bob',
            'what': ['blog']
        }
    }
