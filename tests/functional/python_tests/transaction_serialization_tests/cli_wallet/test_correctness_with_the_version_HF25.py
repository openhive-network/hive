import json

import pytest

from test_tools import paths_to_executables, RemoteNode, Wallet

"""
NO TESTED METHODS
1. 'get_private_key' - less of key in test wallet
2. 'get_private_key_from_password' - less of key in test wallet
3. 'list_my_accounts' - there is no account created with this wallet
4. 'get_conversion_requests' - there is no conversion request in 5m blocklog
5. 'get_collateralized_conversion_requests' - there is no collateralized conversion request in 5m blocklog
6. 'get_withdraw_routes' there is withdraw route in 5m blocklog
7. 'find_recurrent_transfers'  # work but return nothing
8. 'get_open_orders'  # work but return nothing
9. 'get_owner_history' # work but return nothing
10. 'get_encrypted_memo' # no work, less of operation of memo
11. 'list_proposals' # work but return nothing
12. 'find_proposals' # work but return nothing ( there is not proposals
13. 'list_proposal_votes'  # work but return nothing
14. 'help'  # no work
"""


@pytest.fixture
def remote_node_wallet():
    # To allow working on CI, change remote node http_endpoint, ws_endpoint and path to mainnet wallet.
    paths_to_executables.set_path_of('cli_wallet',
                                     '/home/dev/ComparationHF25HF26Mainnet/src/hive_HF26/build/programs/cli_wallet/cli_wallet')
    remote_node = RemoteNode(http_endpoint='0.0.0.0:18091', ws_endpoint='0.0.0.0:18090')
    return Wallet(attach_to=remote_node)


METHODS = [
    ('gethelp', ('get_block',)),
    ('about', ()),
    ('help', ()),
    ('is_new', ()),
    ('is_locked', ()),
    ('list_keys', ()),
    ('list_accounts', ('hiveio', 10)),
    ('list_witnesses', ('gtg', 10)),
    ('get_witness', ('gtg',)),
    ('get_account', ('gtg',)),
    ('get_block', (10,)),
    ('get_ops_in_block', (10, True)),
    ('get_feed_history', ()),
    ('get_account_history', ('gtg', -1, 10)),
    ('get_order_book', (100,)),
    ('get_prototype_operation', ('account_create_operation',)),
    ('get_active_witnesses', ()),
    ('get_transaction', ('82d2c772db5312024f572c9dfbe926e45391f8e9',))
]


@pytest.mark.parametrize(
    'cli_wallet_method, arguments', [
        *METHODS,
    ]
)
def test_comparison_of_get_methods(remote_node_wallet, cli_wallet_method, arguments):
    response_hf26 = read_from_json('output_files_hf26', cli_wallet_method)
    response_from_node = getattr(remote_node_wallet.api, cli_wallet_method)(*arguments)

    assert response_hf26 == response_from_node


def read_from_json(folder_name, method_name):
    with open(f'{folder_name}/{method_name}.pat.json', 'r') as json_file:
        data = json.load(json_file)
        json_file.close()
    return data
