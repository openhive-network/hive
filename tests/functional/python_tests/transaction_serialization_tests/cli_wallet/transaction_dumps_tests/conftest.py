import json
import hashlib
from pathlib import Path
import pytest

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
    assert md5(source_path_file) == md5(target_path_file)

    # Store transaction dumps in folder
    # shutil.move(source_path_file, target_path_file)


def read_bin_from_file(path):
    with open(path, 'r') as file:
        if 'bin' in str(path):
            file_content = file.readlines()
            return file_content

def read_block_log_length_from_json_file(type_of_serialization):
    with open(Path(__file__).parent.joinpath(f'block_log/{type_of_serialization}/block_log_length.json')) as file:
        file = json.load(file)
    block_log_length = file['block_log_length']
    return block_log_length


def md5(fname):
    hash_md5 = hashlib.md5()
    with open(fname, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()