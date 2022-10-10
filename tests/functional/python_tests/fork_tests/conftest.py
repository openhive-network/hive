from pathlib import Path
import time
from typing import Dict, Iterable
import os

import pytest

import test_tools as tt
from test_tools.__private.init_node import InitNode
from test_tools.__private.user_handles.get_implementation import get_implementation

from test_tools.__private.wait_for import wait_for_event

from ..local_tools import init_network
from .local_tools import connect_sub_networks


def run_networks(networks: Iterable[tt.Network], blocklog_directory: Path):
    if blocklog_directory is not None:
        time_offset = tt.Time.read_from_file(blocklog_directory/'timestamp', offset=tt.Time.seconds(-5))
        block_log = tt.BlockLog(None, blocklog_directory/'block_log', include_index=False)

    tt.logger.info('Running nodes...')

    connect_sub_networks(networks)

    nodes = [node for network in networks for node in network.nodes]
    if blocklog_directory is not None:
        nodes[0].run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)
    else:
        nodes[0].run(wait_for_live=False)
    init_node: InitNode = get_implementation(nodes[0])
    endpoint = init_node.get_p2p_endpoint()
    for node in nodes[1:]:
        node.config.p2p_seed_node.append(endpoint)
        if blocklog_directory is not None:
            node.run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)
        else:
            node.run(wait_for_live=False)

    for network in networks:
        network.is_running = True

    deadline = time.time() + InitNode.DEFAULT_WAIT_FOR_LIVE_TIMEOUT
    for node in nodes:
        tt.logger.debug(f'Waiting for {node} to be live...')
        wait_for_event(
            get_implementation(node)._Node__notifications.live_mode_entered_event,
            deadline=deadline,
            exception_message='Live mode not activated on time.'
        )

        tt.logger.info(f'{node}: {node.api.condenser.get_dynamic_global_properties()}')

def display_info(wallet):
    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    tt.logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

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

def prepare_nodes(sub_networks_sizes : list) -> list:
    assert len(sub_networks_sizes) > 0, "At least 1 sub-network is required"

    cnt               = 0
    all_witness_names = []
    sub_networks      = []
    init_node         = None

    for sub_networks_size in sub_networks_sizes:
        tt.logger.info(f'Preparing sub-network nr: {cnt} that consists of {sub_networks_size} witnesses')

        witness_names = [f'witness-{cnt}-{i}' for i in range(sub_networks_size)]
        all_witness_names += witness_names

        sub_network = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
        if cnt == 0:
            init_node = tt.InitNode(network = sub_network)
        sub_networks.append(sub_network)
        tt.WitnessNode(witnesses = witness_names, network = sub_network)
        tt.ApiNode(network = sub_network)

        cnt += 1
    return sub_networks, init_node, all_witness_names

def prepare_sub_networks_generation(sub_networks_sizes : list, block_log_directory_name : str = None) -> Dict:
    sub_networks, init_node, all_witness_names = prepare_nodes(sub_networks_sizes)

    run_networks(sub_networks, None)

    initminer_public_key = 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'
    init_network(init_node, all_witness_names, initminer_public_key, block_log_directory_name)

    return None, None, None

def prepare_sub_networks_launch(sub_networks_sizes : list, block_log_directory_name : str = None) -> Dict:
    sub_networks, init_node, all_witness_names = prepare_nodes(sub_networks_sizes)

    run_networks(sub_networks, Path(block_log_directory_name))

    init_wallet = tt.Wallet(attach_to=init_node)

    display_info(init_wallet)

    return sub_networks, all_witness_names, init_wallet

def prepare_sub_networks(sub_networks_sizes : list, allow_generate_block_log : bool = False, block_log_directory_name : str = None) -> Dict:
    assert block_log_directory_name is not None, "Name of directory with block_log file must be given"

    if allow_generate_block_log:
        return prepare_sub_networks_generation(sub_networks_sizes, block_log_directory_name)
    else:
        return prepare_sub_networks_launch(sub_networks_sizes, block_log_directory_name)

def allow_generate_block_log():
    status = os.environ.get('GENERATE_FORK_BLOCK_LOG', None)
    if status is None:
        return False
    return int(status) == 1

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
