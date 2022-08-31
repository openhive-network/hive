from datetime import datetime, timezone
import time
from typing import Dict

import test_tools as tt
from test_tools.__private.user_handles.get_implementation import get_implementation


BLOCKS_IN_FORK = 5
WAIT_FOR_FORK_TIMEOUT = 90.0


def count_ops_by_type(node, op_type: str, start: int, limit: int = 50):
    """
    :param op_type: type of operation (ex. 'producer_reward_operation')
    :param start: start queries with this block number
    :param limit: limit queries until start-limit+1
    """
    count = {}
    for i in range(start, start-limit, -1):
        response = node.api.account_history.get_ops_in_block(block_num=i, only_virtual=False)
        ops = response["ops"]
        count[i] = 0
        for op in ops:
            this_op_type = op["op"]["type"]
            if this_op_type == op_type:
                count[i] += 1
    return count


def check_account_history_duplicates(node, wallet):
    last_irreversible_block = wallet.api.info()["last_irreversible_block_num"]
    node_reward_operations = count_ops_by_type(node, 'producer_reward_operation', last_irreversible_block, limit=50)
    assert sum(i==1 for i in node_reward_operations.values()) == 50


def assert_no_duplicates(node, *nodes):
    nodes = [node, *nodes]
    for node in nodes:
        wallet = tt.Wallet(attach_to=node)
        check_account_history_duplicates(node, wallet)
    node.wait_number_of_blocks(10)
    for node in nodes:
        wallet = tt.Wallet(attach_to=node)
        check_account_history_duplicates(node, wallet)
    tt.logger.info("No there are no duplicates in account_history.get_ops_in_block...")


def get_time_offset_from_iso_8601(timestamp: str) -> str:
    current_time = datetime.now(timezone.utc)
    new_time = datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=timezone.utc)

    # reduce node start delay from 10s, caused test fails
    difference = round(new_time.timestamp()-current_time.timestamp()) - 5
    time_offset = f'{difference}s'
    return time_offset


def make_fork(networks: Dict[str, tt.Network], at_block=None, blocks_in_fork=BLOCKS_IN_FORK):
    alpha_net = networks['Alpha']
    beta_net = networks['Beta']
    alpha_witness_node = alpha_net.nodes[0]
    beta_witness_node = beta_net.nodes[0]

    fork_block = at_block or get_head_block(alpha_witness_node)
    tt.logger.info(f'Making fork at block {fork_block}')
    if at_block is not None:
        alpha_witness_node.wait_for_block_with_number(at_block)

    alpha_net.disconnect_from(beta_net)

    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(fork_block + blocks_in_fork)
    return fork_block


def reconnect_networks(networks: Dict[str, tt.Network], timeout=WAIT_FOR_FORK_TIMEOUT):
    already_waited = 0
    networks['Alpha'].connect_with(networks['Beta'])
    alpha_endpoint = get_implementation(networks['Alpha'].nodes[0]).get_p2p_endpoint()
    beta_node = networks['Beta'].nodes[0]
    while True:
        connected_peers = beta_node.api.network_node.get_connected_peers()['connected_peers']
        hosts = [ peer['host'] for peer in connected_peers]
        if alpha_endpoint  in hosts:
            break

        if timeout - already_waited <= 0:
            raise TimeoutError('Waited too long for reconnecting p2p networks')

        sleep_time = min(1.0, timeout)
        time.sleep(sleep_time)
        already_waited += sleep_time
    tt.logger.info(f'Reconnected p2p networks after {already_waited} seconds')
    return already_waited


def wait_for_back_from_fork(node, other_node=None, timeout=WAIT_FOR_FORK_TIMEOUT):
    already_waited = 0
    switched_fork_node = None
    while not switched_fork_node:
        if timeout - already_waited <= 0:
            raise TimeoutError('Waited too long for switching forks')

        sleep_time = min(1.0, timeout)
        time.sleep(sleep_time)
        already_waited += sleep_time

        switched_fork_node = switched_fork(node, other_node)

    return (switched_fork_node, already_waited)


def switched_fork(node, other_node=None):
    this_node_number_of_forks = node.get_number_of_forks()
    other_node_number_of_forks = other_node.get_number_of_forks() if other_node else 0
    assert this_node_number_of_forks + other_node_number_of_forks < 2, 'More then one fork encountered'

    if this_node_number_of_forks > 0:
        return node
    if other_node_number_of_forks > 0:
        return other_node

    return False


def get_head_block(node):
    head_block_number = node.api.database.get_dynamic_global_properties()["head_block_number"]
    return head_block_number
