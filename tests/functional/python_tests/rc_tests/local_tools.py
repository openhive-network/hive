import json

from test_tools import logger, Wallet, BlockLog
from datetime import datetime, timezone


BLOCKS_IN_FORK = 5
BLOCKS_AFTER_FORK = 5


def make_fork(world, main_chain_trxs=[], fork_chain_trxs=[]):
    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_witness_node = alpha_net.node('WitnessNode0')
    beta_witness_node = beta_net.node(name='WitnessNode0')

    logger.info(f'Making fork at block {get_head_block(alpha_witness_node)}')

    main_chain_wallet = Wallet(attach_to=alpha_witness_node)
    fork_chain_wallet = Wallet(attach_to=beta_witness_node)
    fork_block = get_head_block(beta_witness_node)
    head_block = fork_block
    alpha_net.disconnect_from(beta_net)

    for trx in main_chain_trxs:
        main_chain_wallet.api.sign_transaction(trx)
    for trx in fork_chain_trxs:
        fork_chain_wallet.api.sign_transaction(trx)

    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK)
    alpha_net.connect_with(beta_net)
    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK + BLOCKS_AFTER_FORK)

    head_block = get_head_block(beta_witness_node)
    return head_block


def wait_for_irreversible_progress(node, block_num):
    logger.info(f'Waiting for progress of irreversible block')
    head_block = get_head_block(node)
    irreversible_block = get_irreversible_block(node)
    logger.info(f"Current head_block_number: {head_block}, irreversible_block_num: {irreversible_block}")
    while irreversible_block < block_num:
        node.wait_for_block_with_number(head_block+1)
        head_block = get_head_block(node)
        irreversible_block = get_irreversible_block(node)
        logger.info(f"Current head_block_number: {head_block}, irreversible_block_num: {irreversible_block}")
    return irreversible_block, head_block


def get_head_block(node):
    head_block_number = node.api.database.get_dynamic_global_properties()["head_block_number"]
    return head_block_number


def get_irreversible_block(node):
    irreversible_block_num = node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]
    return irreversible_block_num


def get_time_offset_from_file(name):
    timestamp = ''
    with open(name, 'r') as f:
        timestamp = f.read()
    timestamp = timestamp.strip()
    current_time = datetime.now(timezone.utc)
    new_time = datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=timezone.utc)
    difference = round(new_time.timestamp()-current_time.timestamp()) - 10 # circa 10 seconds is needed for nodes to startup
    time_offset = str(difference) + 's'
    return time_offset


def run_networks(world, blocklog_directory):
    time_offset = get_time_offset_from_file(blocklog_directory/'timestamp')

    block_log = BlockLog(None, blocklog_directory/'block_log', include_index=False)

    logger.info('Running nodes...')

    nodes = world.nodes()
    nodes[0].run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)
    endpoint = nodes[0].get_p2p_endpoint()
    for node in nodes[1:]:
        node.config.p2p_seed_node.append(endpoint)
        node.run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)

    for network in world.networks():
        network.is_running = True
    for node in nodes:
        node.wait_for_live()


def create_node_with_database(network, url):
    api_node = network.create_api_node()
    api_node.config.plugin.append('sql_serializer')
    api_node.config.psql_url = str(url)
    return api_node
