from test_tools import logger, Account, World, Asset, BlockLog
from local_tools import generate, ProducingBlocksInBackgroundContext
import time
import sqlalchemy
import math
import sys
import signal


FIRST_BLOCK_TIMESTAMP = int(time.time()) - 200 * 3
BLOCKS_IN_FORK = 10
SYNC_AFTER_FORK = 10
FORKS = 1

def test_fork_and_compare_with_reference_node(world):
    #GIVEN
    block_log = prepare_block_log(world)
    node_under_test, reference_node = prepare_nodes(world, block_log)

    engine_under_test = sqlalchemy.create_engine('postgresql://myuser:mypassword@localhost/haf_block_log', echo=True)
    engine_reference = sqlalchemy.create_engine('postgresql://myuser:mypassword@localhost/haf_block_log2', echo=True)
    database_under_test = engine_under_test.connect()
    database_reference = engine_reference.connect()

    # WHEN
    fork_block = 205
    logger.info(f'making fork at block {fork_block}')
    node_under_test.wait_for_block_with_number(fork_block)
    make_fork(world)
    wait_for_irreversible_progress(node_under_test, fork_block + BLOCKS_IN_FORK * 3)

    # THEN
    result = database_under_test.execute(f'select * from hive.blocks order by num')
    result2 = database_reference.execute(f'select * from hive.blocks order by num')
    logger.info('database_under_test content')
    blocks_node_under_test = {}
    for row in result:
        # logger.info(row)
        num = row['num']
        hash = row['hash'].hex()
        logger.info(f'{num} {hash}')
        blocks_node_under_test[num] = hash
    logger.info('database_reference content')
    blocks_refrence_node = {}
    for row in result2:
        # logger.info(row)
        num = row['num']
        hash = row['hash'].hex()
        logger.info(f'{num} {hash}')
        blocks_refrence_node[num] = hash
    
    for i in range(1, fork_block + BLOCKS_IN_FORK * 3):
        logger.info(f'for block {i}')
        print(f'database_under_test hash: \t{blocks_node_under_test[i]}')
        print(f'database_reference hash: \t{blocks_refrence_node[i]}')
        # assert blocks_node_under_test[i] == blocks_refrence_node[i]

    # WAIT FOR SIGINT
    try:
        logger.info("test successful, press CTRL+C")
        while True:
            time.sleep(1)
    except:
        pass


def make_fork(world):
    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_witness_node = alpha_net.node('WitnessNode0')
    beta_witness_node = beta_net.node(name='WitnessNode0')


    wallet = beta_witness_node.attach_wallet()
    fork_block = beta_witness_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    head_block = fork_block
    alpha_net.disconnect_from(beta_net)

    with wallet.in_single_transaction():
        name = "dummy-acnt-beta"
        wallet.api.create_account('initminer', name, '')
        logger.info('create_account created...')
    logger.info('create_account sent...')

    alpha_witness_node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK)
    beta_witness_node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK)
    alpha_net.connect_with(beta_net)
    alpha_witness_node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK + SYNC_AFTER_FORK)
    beta_witness_node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK + SYNC_AFTER_FORK)


def wait_for_irreversible_progress(node, block_num):
    head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    while irreversible_block_num < block_num:
        node.wait_for_block_with_number(head_block_number+1)
        head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
        irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    


def prepare_block_log(world):
    alpha_witness_names = [f'witness{i}-alpha' for i in range(11)]
    beta_witness_names = [f'witness{i}-beta' for i in range(10)]
    all_witness_names = alpha_witness_names + beta_witness_names

    # Create first network
    debug_net = world.create_network('BlockLogNet')
    debug_node = debug_net.create_api_node(name='BlockLogNode')

    # Run
    logger.info('Running network, NOT waiting for live...')
    debug_net.run(wait_for_live=False)

    timestamp = FIRST_BLOCK_TIMESTAMP
    debug_node.timestamp = timestamp

    for i in range(128):
        generate(debug_node, 0.03)

    logger.info('Attaching wallet to debug node...')
    wallet = debug_node.attach_wallet(timeout=60)

    with ProducingBlocksInBackgroundContext(debug_node, 1):
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.create_account('initminer', name, '')
            logger.info('create_account created...')
    logger.info('create_account sent...')


    with ProducingBlocksInBackgroundContext(debug_node, 1):
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))
            logger.info('transfer_to_vesting created...')
    logger.info('transfer_to_vesting sent...')


    with ProducingBlocksInBackgroundContext(debug_node, 1):
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.update_witness(
                    name, "https://" + name,
                    Account(name).public_key,
                    {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
                )
            logger.info('update_witness created...')
    logger.info('update_witness sent...')

    while debug_node.timestamp < time.time():
        generate(debug_node, 0.03)

    logger.info("Witness state after voting")
    response = debug_node.api.database.list_witnesses(start=0, limit=100, order='by_name')
    active_witnesses = response["result"]["witnesses"]
    active_witnesses_names = [witness["owner"] for witness in active_witnesses]
    logger.info(active_witnesses_names)
    assert len(active_witnesses_names) == 22

    return debug_node.get_block_log()


def prepare_nodes(world, block_log):
    alpha_witness_names = [f'witness{i}-alpha' for i in range(12)]
    beta_witness_names = [f'witness{i}-beta' for i in range(8)]

    alpha_net = world.create_network('Alpha')
    alpha_witness_node = alpha_net.create_witness_node(witnesses=alpha_witness_names)
    reference_node = alpha_net.create_api_node(name = 'ReferenceNode')
    beta_net = world.create_network('Beta')
    beta_witness_node = beta_net.create_witness_node(witnesses=beta_witness_names)
    node_under_test = beta_net.create_api_node(name = 'NodeUnderTest')

    alpha_witness_node.config.enable_stale_production = True
    alpha_witness_node.config.required_participation = 0
    beta_witness_node.config.enable_stale_production = True
    beta_witness_node.config.required_participation = 0

    node_under_test.config.plugin.append('sql_serializer')
    node_under_test.config.psql_url = 'postgresql://myuser:mypassword@localhost/haf_block_log'
    reference_node.config.plugin.append('sql_serializer')
    reference_node.config.psql_url = 'postgresql://myuser:mypassword@localhost/haf_block_log2'

    #RUN
    logger.info('Running ndoes...')

    alpha_witness_node.run(timeout=math.inf, replay_from=block_log)
    endpoint = alpha_witness_node.config.p2p_endpoint

    for node in [reference_node, beta_witness_node, node_under_test]:
        node.config.p2p_seed_node.append(endpoint)
        node.run(timeout=math.inf, replay_from=block_log)

    alpha_net.is_running = True
    beta_net.is_running = True

    for node in [alpha_witness_node, reference_node, beta_witness_node, node_under_test]:
        node.wait_for_block_with_number(205)

    return node_under_test, reference_node
