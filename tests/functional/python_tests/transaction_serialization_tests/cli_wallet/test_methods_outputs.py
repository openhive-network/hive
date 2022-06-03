import json
import pathlib

import pytest

from test_tools import paths_to_executables, RemoteNode, Wallet

STORE_PATTERNS = False


@pytest.fixture
def remote_node_wallet():
    # To allow working on CI, change remote node http_endpoint, ws_endpoint and path to mainnet wallet.
    paths_to_executables.set_path_of('cli_wallet',
                                     '/home/dev/ComparationHF25HF26Mainnet/src/hive_HF26/build/programs/cli_wallet/cli_wallet')
    remote_node = RemoteNode(http_endpoint='0.0.0.0:18091', ws_endpoint='0.0.0.0:18090')
    return Wallet(attach_to=remote_node)


WALLET_API_METHODS = [
    ('gethelp', ('get_block',)),
    # ('about', ()),  #  should not be tested, output is changing in time
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
    ('get_transaction', ('82d2c772db5312024f572c9dfbe926e45391f8e9',)),
    # ('get_private_key', ()),  # unknown key, cause exception
    # ('get_private_key_from_password', ()),  # unknown key, cause exception
    ('list_my_accounts', ()),  # work but return nothing ( there no acccount created with this wallet)
    ('get_conversion_requests', ('gtg',)),  # work but return nothing
    ('get_collateralized_conversion_requests', ('gtg',)),  # work but return nothing
    ('get_withdraw_routes', ('gtg', 'all')),  # work but return nothing
    ('find_recurrent_transfers', ('gtg',)),  # work but return nothing
    ('get_open_orders', ('gtg',)),  # work but return nothing
    ('get_owner_history', ('gtg',)),  # work but return nothing
    ('get_encrypted_memo', ('gtg', 'blocktrades', 'memo')),  # return 'memo', less of operation of memo
    ('get_prototype_operation', ('account_create_operation',)),  # work
    ('list_proposals', ([""], 100, 30, 0, 0)),  # work but return nothing (here is no proposals)
    ('find_proposals', ([0],)),  # work but return nothing (there is no proposals)
    ('list_proposal_votes', ([""], 100, 33, 0, 0)),  # work but return nothing (there is no proposals)
    ('info', ()),
]


@pytest.mark.parametrize(
    'cli_wallet_method, arguments', WALLET_API_METHODS
)
def test_or_dump_methods_outputs(remote_node_wallet, cli_wallet_method, arguments):
    patterns_directory = 'response_patterns'
    response = getattr(remote_node_wallet.api, cli_wallet_method)(*arguments)

    if STORE_PATTERNS:
        write_to_json(patterns_directory, cli_wallet_method, response)
    else:
        pattern = read_from_json(patterns_directory, cli_wallet_method)
        assert response == pattern


def read_from_json(folder_name, method_name):
    path_to_folder = pathlib.Path(__file__).parent.absolute() / folder_name
    with open(f'{path_to_folder}/{method_name}.pat.json', 'r') as json_file:
        return json.load(json_file)


def write_to_json(folder_name, method_name, json_response):
    path_to_folder = pathlib.Path(__file__).parent.absolute() / folder_name
    path_to_folder.mkdir(parents=True, exist_ok=True)
    with open(f'{path_to_folder}/{method_name}.pat.json', 'w') as json_file:
        json.dump(json_response, json_file)
