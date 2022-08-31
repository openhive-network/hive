from datetime import datetime, timezone
from pathlib import Path
import time
from typing import Dict, Iterable

import pytest

import test_tools as tt
from test_tools.__private.init_node import InitNode
from test_tools.__private.user_handles.get_implementation import get_implementation

from test_tools.__private.wait_for import wait_for_event
from tests.functional.python_tests.fork_tests.local_tools import get_time_offset_from_iso_8601


def get_time_offset_from_file(file: Path):
    with open(file, 'r') as f:
        timestamp = f.read()
    timestamp = timestamp.strip()
    time_offset = get_time_offset_from_iso_8601(timestamp)
    return time_offset


def run_networks(networks: Iterable[tt.Network], blocklog_directory: Path):
    time_offset = get_time_offset_from_file(blocklog_directory/'timestamp')
    block_log = tt.BlockLog(None, blocklog_directory/'block_log', include_index=False)

    tt.logger.info('Running nodes...')

    nodes = [node for network in networks for node in network.nodes]
    nodes[0].run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)
    init_node: InitNode = get_implementation(nodes[0])
    endpoint = init_node.get_p2p_endpoint()
    for node in nodes[1:]:
        node.config.p2p_seed_node.append(endpoint)
        node.run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)

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

    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    tt.logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

    yield {
        'Alpha': alpha_net,
        'Beta': beta_net,
    }


@pytest.fixture
def prepared_networks_witnesses_18_2() -> Dict:
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
    tt.WitnessNode(witnesses=alpha_witness_names+beta_witness_names[:8], network=alpha_net)

    # Create second network
    beta_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    tt.WitnessNode(witnesses=beta_witness_names[8:], network=beta_net)

    blocklog_directory = Path(__file__).parent / 'block_log'
    run_networks([alpha_net, beta_net], blocklog_directory)

    wallet = tt.Wallet(attach_to=alpha_net.nodes[0])

    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    tt.logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

    yield {
        'Alpha': alpha_net,
        'Beta': beta_net,
    }
