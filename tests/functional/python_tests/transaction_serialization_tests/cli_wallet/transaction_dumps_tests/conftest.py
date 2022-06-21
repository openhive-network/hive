from pathlib import Path
import pytest
import shutil

import test_tools as tt


@pytest.fixture
def node():
    init_node = tt.InitNode()
    init_node.run()
    return init_node


@pytest.fixture
def legacy_wallet(node, request):
    wallet = tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                             '--transaction-serialization=legacy'])
    yield wallet

    for file_extension in ['json', 'bin']:
        source_path_json_file = wallet.directory / f'{request.fspath.purebasename}.{file_extension}'
        target_path_json_file = Path(
            request.fspath.dirname) / f'dumped_{file_extension}_files_legacy_wallet' / f'{request.fspath.purebasename}.json'
        shutil.move(source_path_json_file, target_path_json_file)


@pytest.fixture
def nai_wallet(node, request):
    wallet = tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                             '--transaction-serialization=hf26'])
    yield wallet

    for file_extension in ['json', 'bin']:
        source_path_json_file = wallet.directory / f'{request.fspath.purebasename}.{file_extension}'
        target_path_json_file = Path(
            request.fspath.dirname) / f'dumped_{file_extension}_files_nai_wallet' / f'{request.fspath.purebasename}.json'
        shutil.move(source_path_json_file, target_path_json_file)


@pytest.fixture(params=['legacy_wallet', 'nai_wallet'])
def wallet(request):
    return request.getfixturevalue(request.param)
