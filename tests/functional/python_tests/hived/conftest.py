import pytest
from pathlib import Path
from typing import Tuple
from typing import Iterable, List

import test_tools as tt


def pytest_addoption(parser: pytest.Parser):
    parser.addoption( "--ref", action="store", type=str, help='specifies address of reference node', default=None)
    parser.addoption( "--test", action="store", type=str, help='specifies address of tested service', default=None)
    parser.addoption( "--hashes", action="store", type=str, help='specifies path to file with hashes to check (one per line)', default=None)

@pytest.fixture(scope='package')
def block_log_helper() -> Tuple[tt.BlockLog, int]:
    BLOCK_COUNT = 30

    tt.logger.info(f'preparing block log with {BLOCK_COUNT} blocks')
    node = tt.InitNode()
    node.run(wait_for_live=True)

    node.wait_for_block_with_number(BLOCK_COUNT)
    block_log_length = node.get_last_block_number()

    node.close()
    tt.logger.info(f'prepared block log with {block_log_length} blocks')
    yield node.block_log, block_log_length


@pytest.fixture(scope='package')
def block_log(block_log_helper: Tuple[tt.BlockLog, int]) -> Path:
    return block_log_helper[0].path

@pytest.fixture(scope='package')
def block_log_length(block_log_helper: Tuple[tt.BlockLog, int]) -> int:
    return block_log_helper[1]

@pytest.fixture
def request_limit() -> int:
    return 1_000

@pytest.fixture
def ref_node(request: pytest.FixtureRequest) -> tt.RemoteNode:
    address_of_ref_node = request.config.getoption("--ref")
    assert address_of_ref_node is not None, "Please specify --ref flag with address to reference node"
    return tt.RemoteNode(address_of_ref_node)

@pytest.fixture
def test_node(request: pytest.FixtureRequest) -> tt.RemoteNode:
    address_of_test_hived = request.config.getoption("--test")
    assert address_of_test_hived is not None, "Please specify --test flag with address to node that will be tested"
    return tt.RemoteNode(address_of_test_hived)

@pytest.fixture
def transactions(request: pytest.FixtureRequest) -> List[str]:
    path_to_hashes = request.config.getoption("--hashes")
    assert path_to_hashes is not None, "Path to file with transaction hashes has not been specified, use --hashes flag"

    path_to_hashes: Path = Path(path_to_hashes)
    assert path_to_hashes.exists()
    with path_to_hashes.open('rt') as file:
        return [x.strip() for x in file.readlines()]

@pytest.fixture
def accounts(ref_node: tt.RemoteNode, request_limit: int) -> List[str]:
    all_accounts = []
    result_from_list_accounts_call = [i for i in range(request_limit)]
    last_account_start = None

    while len(result_from_list_accounts_call) < request_limit:
        result_from_list_accounts_call = ref_node.api.database.list_accounts(start=last_account_start, order="by_name" ,limit=request_limit)['result']
        all_accounts.extend([ x['name'] for x in result_from_list_accounts_call['accounts'] ])
        last_account_start = all_accounts[-1]

    assert len(all_accounts)
    return all_accounts

@pytest.fixture
def block_range() -> Iterable[int]:
    return range(4_900_000, 4_905_000)
