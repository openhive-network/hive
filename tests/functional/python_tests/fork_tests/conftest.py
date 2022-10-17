
from pathlib import Path
from typing import Dict

import pytest

import test_tools as tt

from ..local_tools import run_networks, display_info, prepare_sub_networks, allow_generate_block_log

@pytest.fixture
def prepared_networks() -> Dict:
    """
    Fixture consists of 1 init node, 8 witness nodes and 2 api nodes.
    After fixture creation there are 21 active witnesses, and last irreversible block
    is behind head block like in real network.
    """

    tt.logger.info('Preparing fixture prepared_networks')
    alpha_witness_names = [f'witness{i}-alpha' for i in range(10)]
    beta_witness_names = [f'witness{i}-beta' for i in range(10)]
    all_witness_names = alpha_witness_names + beta_witness_names

    # Create first network
    alpha_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    init_node = tt.InitNode(network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(0, 3)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(3, 6)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(6, 8)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(8, 10)], network=alpha_net)
    alpha_api_node = tt.ApiNode(network=alpha_net)

    # Create second network
    beta_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(0, 3)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(3, 6)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(6, 8)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(8, 10)], network=beta_net)
    tt.ApiNode(network=beta_net)

    blocklog_directory = Path(__file__).parent / 'block_log'
    run_networks([alpha_net, beta_net], blocklog_directory)

    wallet = tt.Wallet(attach_to=init_node)

    display_info(wallet)

    yield {
        'Alpha': alpha_net,
        'Beta': beta_net,
    }

def create_block_log_directory_name(name : str):
    return str(Path(__file__).parent.absolute()) + '/' + name

@pytest.fixture
def prepare_sub_networks_10_11() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11]) }

@pytest.fixture
def prepare_fork_2_sub_networks_00() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([3, 18], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_00')) }

@pytest.fixture
def prepare_fork_2_sub_networks_01() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([6, 17], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_01')) }

@pytest.fixture
def prepare_fork_2_sub_networks_02() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([6, 17], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_02')) }

@pytest.fixture
def prepare_fork_2_sub_networks_03() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([3, 18], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_03')) }

@pytest.fixture
def prepare_obi_throw_exception_00() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11], allow_generate_block_log(), create_block_log_directory_name('block_log_obi_throw_exception_00')) }

@pytest.fixture
def prepare_obi_throw_exception_01() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11], allow_generate_block_log(), create_block_log_directory_name('block_log_obi_throw_exception_01')) }

@pytest.fixture
def prepare_obi_throw_exception_02() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11], allow_generate_block_log(), create_block_log_directory_name('block_log_obi_throw_exception_02')) }

@pytest.fixture
def prepare_fork_3_sub_networks_00() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([3, 4, 14], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_3_sub_networks_00')) }

@pytest.fixture
def prepare_fork_3_sub_networks_01() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([7, 7, 7], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_3_sub_networks_01')) }
