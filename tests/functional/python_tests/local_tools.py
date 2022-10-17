from datetime import datetime, timezone
from pathlib import Path
import time
from typing import Iterable


from typing import List, Dict

from pathlib import Path

import os

import test_tools as tt

from test_tools.__private.wait_for import wait_for_event
from test_tools.__private.init_node import InitNode
from test_tools.__private.user_handles.get_implementation import get_implementation
from tests.functional.python_tests.fork_tests.local_tools import get_time_offset_from_iso_8601

def allow_generate_block_log():
    status = os.environ.get('GENERATE_FORK_BLOCK_LOG', None)
    if status is None:
        return False
    return int(status) == 1

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

def display_info(wallet):
    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    tt.logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

def parse_datetime(datetime_: str) -> datetime:
    return datetime.strptime(datetime_, '%Y-%m-%dT%H:%M:%S')

def get_time_offset_from_file(file: Path):
    with open(file, 'r') as f:
        timestamp = f.read()
    timestamp = timestamp.strip()
    time_offset = get_time_offset_from_iso_8601(timestamp)
    return time_offset

def connect_sub_networks(sub_networks : list):
    assert len(sub_networks) > 1

    current_idx = 0
    while current_idx < len(sub_networks) - 1:
        next_current_idx = current_idx + 1
        while next_current_idx < len(sub_networks):
            tt.logger.info(f"Sub network {current_idx} connected with {next_current_idx}")
            sub_networks[current_idx].connect_with(sub_networks[next_current_idx])
            next_current_idx += 1
        current_idx += 1

def disconnect_sub_networks(sub_networks : list):
    assert len(sub_networks) > 1

    current_idx = 0
    while current_idx < len(sub_networks) - 1:
        next_current_idx = current_idx + 1
        while next_current_idx < len(sub_networks):
            tt.logger.info(f"Sub network {current_idx} disconnected from {next_current_idx}")
            sub_networks[current_idx].disconnect_from(sub_networks[next_current_idx])
            next_current_idx += 1
        current_idx += 1

def init_network( init_node, all_witness_names : List[str], key : str = None, block_log_directory_name : str = None):

    tt.logger.info('Attaching wallets...')
    wallet = tt.Wallet(attach_to=init_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    tt.logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
    init_node.wait_for_block_with_number(43)

    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            if key is None:
                wallet.api.create_account('initminer', name, '')
            else:
                wallet.api.create_account_with_keys('initminer', name, '', key, key, key, key)
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.update_witness(
                name, "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    tt.logger.info('Wait 21 blocks to schedule newly created witnesses into future slate')
    init_node.wait_number_of_blocks(21)
    tt.logger.info('Wait 21 blocks for future slate to become active slate')
    init_node.wait_number_of_blocks(21)

    active_witnesses = init_node.api.database.get_active_witnesses()["witnesses"]
    tt.logger.info(f"Witness state after voting: {active_witnesses}")
    assert len(active_witnesses) == 21

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info('Wait 21 blocks (when every witness sign at least one block)')
    init_node.wait_number_of_blocks(21)

    result = wallet.api.info()
    head_block_num = result['head_block_number']
    timestamp = init_node.api.block.get_block(block_num=head_block_num)['block']['timestamp']
    tt.logger.info(f'head block timestamp: {timestamp}')

    if block_log_directory_name is not None:
        if os.path.exists(block_log_directory_name):
            Path(block_log_directory_name + '/block_log').unlink(missing_ok=True)
        else:
            os.mkdir(block_log_directory_name)

        init_node.close()
        init_node.get_block_log(include_index=False).copy_to(block_log_directory_name)

        with open(block_log_directory_name + '/timestamp', 'w') as f:
            f.write(f'{timestamp}')

    return wallet

def run_networks(networks: Iterable[tt.Network], blocklog_directory: Path):
    if blocklog_directory is not None:
        time_offset = get_time_offset_from_file(blocklog_directory/'timestamp')
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
