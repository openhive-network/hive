from sqlalchemy_utils import database_exists, create_database, drop_database

from test_tools import logger, Wallet


BLOCKS_IN_FORK = 5
BLOCKS_AFTER_FORK = 10


def make_fork(world, fork_number=0):
    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_witness_node = alpha_net.node('WitnessNode0')
    beta_witness_node = beta_net.node(name='WitnessNode0')

    wallet = Wallet(attach_to=beta_witness_node)
    fork_block = beta_witness_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    head_block = fork_block
    alpha_net.disconnect_from(beta_net)

    with wallet.in_single_transaction():
        name = "dummy-acnt-" + str(fork_number)
        wallet.api.create_account('initminer', name, '')

    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK)
    alpha_net.connect_with(beta_net)
    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK + BLOCKS_AFTER_FORK)

    head_block = beta_witness_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    return head_block


def wait_for_irreversible_progress(node, block_num):
    head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    while irreversible_block_num < block_num:
        node.wait_for_block_with_number(head_block_number+1)
        head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
        irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]


def prepare_nodes(world, block_log, start_testnet_block_number):
    alpha_witness_names = [f'witness{i}-alpha' for i in range(12)]
    beta_witness_names = [f'witness{i}-beta' for i in range(8)]

    alpha_net = world.create_network('Alpha')
    alpha_witness_node = alpha_net.create_witness_node(witnesses=alpha_witness_names)
    beta_net = world.create_network('Beta')
    beta_witness_node = beta_net.create_witness_node(witnesses=beta_witness_names)
    node_under_test = beta_net.create_api_node(name = 'NodeUnderTest')

    alpha_witness_node.config.enable_stale_production = True
    alpha_witness_node.config.required_participation = 0
    beta_witness_node.config.enable_stale_production = True
    beta_witness_node.config.required_participation = 0

    node_under_test.config.plugin.append('sql_serializer')
    node_under_test.config.psql_url = 'postgresql://myuser:mypassword@localhost/haf_block_log'

    #RUN
    logger.info('Running nodes...')

    alpha_witness_node.run(wait_for_live=False, replay_from=block_log)
    endpoint = alpha_witness_node.get_p2p_endpoint()

    for node in [beta_witness_node, node_under_test]:
        node.config.p2p_seed_node.append(endpoint)
        node.run(wait_for_live=False, replay_from=block_log)

    alpha_net.is_running = True
    beta_net.is_running = True

    for node in [alpha_witness_node, beta_witness_node, node_under_test]:
        node.wait_for_block_with_number(start_testnet_block_number)

    return node_under_test


def wait_for_irreversible_progress(node, block_num):
    head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    while irreversible_block_num < block_num:
        node.wait_for_block_with_number(head_block_number+1)
        head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
        irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
        logger.info(f"head block is {head_block_number}, last irreversible block is {irreversible_block_num}")


def prepare_database(engine):
    if database_exists(engine.url):
        drop_database(engine.url)
    create_database(engine.url)
    with engine.connect() as connection:
        connection.execute('CREATE EXTENSION hive_fork_manager')
