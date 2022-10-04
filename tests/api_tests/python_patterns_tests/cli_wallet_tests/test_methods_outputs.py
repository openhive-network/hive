import json
import pathlib
from typing import Any, Dict

import pytest

import test_tools as tt

STORE_PATTERNS = False

__PATTERNS_DIRECTORY = pathlib.Path(__file__).with_name('response_patterns')


@pytest.fixture
def http_endpoint(request):
    return request.config.getoption("--http-endpoint")


@pytest.fixture
def ws_endpoint(request):
    return request.config.getoption("--ws-endpoint")


@pytest.fixture
def wallet_path(request):
    return request.config.getoption("--wallet-path")


@pytest.fixture
def remote_node_wallet(http_endpoint, ws_endpoint, wallet_path):
    # To allow working on CI, change remote node http_endpoint, ws_endpoint and path to mainnet wallet.
    tt.paths_to_executables.set_path_of('cli_wallet', wallet_path)
    remote_node = tt.RemoteNode(http_endpoint=http_endpoint, ws_endpoint=ws_endpoint)
    return tt.Wallet(attach_to=remote_node)


WALLET_API_METHODS = [
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
    ('list_rc_direct_delegations', (['gtg', 'blocktrades'], 100)),
    ('list_rc_accounts', ('gtg', 100)),
    ('find_rc_accounts', (['gtg'],)),
]


@pytest.mark.mainnet_5m
@pytest.mark.parametrize(
    'cli_wallet_method, arguments', WALLET_API_METHODS
)
def test_or_dump_methods_outputs(remote_node_wallet, cli_wallet_method, arguments):
    response = getattr(remote_node_wallet.api, cli_wallet_method)(*arguments)

    if STORE_PATTERNS:
        write_to_json_pattern(__PATTERNS_DIRECTORY, cli_wallet_method, response)
    else:
        pattern = read_from_json_pattern(__PATTERNS_DIRECTORY, cli_wallet_method)
        assert response == pattern


def read_from_json_pattern(directory: pathlib.Path, method_name: str) -> Dict[str, Any]:
    with open(directory / f'{method_name}.pat.json', 'r') as json_file:
        return json.load(json_file)


def write_to_json_pattern(directory: pathlib.Path, method_name: str, json_response) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / f'{method_name}.pat.json', 'w') as json_file:
        json.dump(json_response, json_file)
