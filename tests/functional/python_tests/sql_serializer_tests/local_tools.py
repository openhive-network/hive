from test_tools import logger, Account, Asset
import time
import threading
import math


BLOCKS_IN_FORK = 5
BLOCKS_AFTER_FORK = 10


def make_fork(world, fork_number=0):
    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_witness_node = alpha_net.node('WitnessNode0')
    beta_witness_node = beta_net.node(name='WitnessNode0')

    wallet = beta_witness_node.attach_wallet()
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
    

def prepare_block_log(world, blocklog_length):
    # TODO: cut block log
    first_block_timestamp = int(time.time()) - blocklog_length * 3 - 15 * 3 # 20 * 3 -= average time to make block irreversible

    alpha_witness_names = [f'witness{i}-alpha' for i in range(12)]
    beta_witness_names = [f'witness{i}-beta' for i in range(8)]
    all_witness_names = alpha_witness_names + beta_witness_names

    # Create first network
    debug_net = world.create_network('BlockLogNet')
    debug_node = debug_net.create_api_node(name='BlockLogNode')

    # Run
    logger.info('Running network, NOT waiting for live...')
    debug_net.run(wait_for_live=False)

    timestamp = first_block_timestamp
    debug_node.timestamp = timestamp

    for i in range(blocklog_length//2):
        generate(debug_node, 0.03)

    logger.info('Attaching wallet to debug node...')
    wallet = debug_node.attach_wallet(timeout=60)

    with ProducingBlocksInBackgroundContext(debug_node, 1):
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.create_account('initminer', name, '')


    with ProducingBlocksInBackgroundContext(debug_node, 1):
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))


    with ProducingBlocksInBackgroundContext(debug_node, 1):
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.update_witness(
                    name, "https://" + name,
                    Account(name).public_key,
                    {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
                )

    while debug_node.timestamp < time.time():
        generate(debug_node, 0.03)

    logger.info("Witness state after voting")
    response = debug_node.api.database.list_witnesses(start=0, limit=100, order='by_name')
    active_witnesses = response["result"]["witnesses"]
    active_witnesses_names = [witness["owner"] for witness in active_witnesses]
    logger.info(active_witnesses_names)
    assert len(active_witnesses_names) == 21

    return debug_node.get_block_log()


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

    alpha_witness_node.run(timeout=math.inf, replay_from=block_log)
    endpoint = alpha_witness_node.get_p2p_endpoint()

    for node in [beta_witness_node, node_under_test]:
        node.config.p2p_seed_node.append(endpoint)
        node.run(timeout=math.inf, replay_from=block_log)

    alpha_net.is_running = True
    beta_net.is_running = True

    for node in [alpha_witness_node, beta_witness_node, node_under_test]:
        node.wait_for_block_with_number(start_testnet_block_number)
    logger.info('Finished running nodes...')

    return node_under_test


def wait_for_irreversible_progress(node, block_num):
    head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    while irreversible_block_num < block_num:
        node.wait_for_block_with_number(head_block_number+1)
        head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
        irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]


def generate(debug_node, block_interval):
    global timestamp
    time.sleep(block_interval)
    response = debug_node.api.debug_node.debug_get_scheduled_witness( slot = 1 )
    scheduled_witness = response['result']['scheduled_witness']
    logger.info(f'{debug_node}: will generate block with {scheduled_witness} key')
    debug_node.api.debug_node.debug_generate_blocks(
        debug_key=Account(scheduled_witness).private_key,
        count = 1,
        skip = 0,
        miss_blocks = 0,
        edit_if_needed = False,
        timestamp = debug_node.timestamp or 0,
        broadcast = True
    )
    if debug_node.timestamp:
        debug_node.timestamp += 3


class ProducingBlocksInBackgroundContext:
    def __init__(self, debug_node, block_interval):
        self.__debug_node = debug_node
        self.__block_interval = block_interval
        self.__running = False

    def __enter__(self):
        self.__thread = threading.Thread(target = self.__generating_loop)
        self.__running = True
        self.__thread.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__running = False
        self.__thread.join()

    def __generating_loop(self):
        while self.__running:
            generate(self.__debug_node, self.__block_interval)
