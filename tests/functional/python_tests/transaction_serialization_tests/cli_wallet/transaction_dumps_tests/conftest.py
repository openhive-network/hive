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
def wallet(node, request):

    wallet = tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}'])
    yield wallet

    for file_extension in ['json', 'bin']:
        source_path_json_file = wallet.directory / f'{request.fspath.purebasename}.{file_extension}'
        target_path_json_file = Path(request.fspath.dirname) / f'dumped_{file_extension}_files' / f'{request.fspath.purebasename}.json'
        shutil.move(source_path_json_file, target_path_json_file)
