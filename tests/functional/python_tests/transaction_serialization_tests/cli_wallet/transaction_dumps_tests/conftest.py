import filecmp
import json
from pathlib import Path
import pytest
import shutil

import test_tools as tt

from .local_tools import get_type_of_serialization_from_test_name

@pytest.fixture
def node():
    init_node = tt.InitNode()
    init_node.run()
    return init_node


@pytest.fixture
def replayed_node(request):
    init_node = tt.InitNode()
    type_of_serialization = get_type_of_serialization_from_test_name(request.keywords.node.name)
    block_log_length = read_block_log_length_from_json_file(type_of_serialization)
    init_node.run(replay_from=Path(__file__).parent.joinpath(f'block_log/{type_of_serialization}/block_log'), wait_for_live=False, stop_at_block=block_log_length)
    return init_node

# @pytest.fixture
# def legacy_wallet(node, request):
#     wallet = tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
#                                                              '--transaction-serialization=legacy'])
#     yield wallet
#
#     for file_extension in ['json', 'bin']:
#         source_path_json_file = wallet.directory / f'{request.fspath.purebasename}.{file_extension}'
#         target_path_json_file = Path(
#             request.fspath.dirname) / f'dumped_{file_extension}_files_legacy_wallet' / f'{request.fspath.purebasename}.json'
#         shutil.move(source_path_json_file, target_path_json_file)
#
#
@pytest.fixture
def nai_wallet(replayed_node, request):
    wallet = tt.Wallet(attach_to=replayed_node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                             '--transaction-serialization=hf26'])
    yield wallet

    for file_extension in ['json', 'bin']:
        source_path_json_file = wallet.directory / f'{request.fspath.purebasename}.{file_extension}'
        target_path_json_file = Path(
            request.fspath.dirname) / f'dumped_{file_extension}_files_hf26_wallet' / f'{request.fspath.purebasename}.{file_extension}'
        shutil.move(source_path_json_file, target_path_json_file)
#
#
# @pytest.fixture(params=['legacy_wallet', 'nai_wallet'])
# def wallet(request):
#     return request.getfixturevalue(request.param)


@pytest.fixture(params=['legacy', 'hf26'])
def wallet(replayed_node, request):
    type_of_serialization = request.param
    wallet = tt.Wallet(attach_to=replayed_node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                             f'--transaction-serialization={type_of_serialization}'])

    yield wallet, type_of_serialization

    for file_extension in ['json', 'bin']:
        source_path_file = wallet.directory / f'{request.fspath.purebasename}.{file_extension}'
        target_path_file = Path(
            request.fspath.dirname) / f'dumped_{file_extension}_files_{type_of_serialization}_wallet' / f'{request.fspath.purebasename}.{file_extension}'


    # Compare actual transaction with stored transaction
        stored_transaction = read_bin_from_file(target_path_file)
        actual_transaction = read_bin_from_file(source_path_file)

        # assert stored_transaction == actual_transaction

    # Store transaction dumps in folder
    #     shutil.move(source_path_file, target_path_file)


def read_bin_from_file(path):
    with open(path, 'r') as file:
        if 'json' in str(path):
            file_content = file.readlines()
            return file_content

def read_block_log_length_from_json_file(type_of_serialization):
    with open(Path(__file__).parent.joinpath(f'block_log/{type_of_serialization}/block_log_length.json')) as file:
        file = json.load(file)
    block_log_length = file['block_log_length']
    return block_log_length