from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import itertools
from pathlib import Path
import time
from threading import Event

from test_tools import logger, Asset, Wallet, BlockLog, Account
from test_tools.private.wait_for import wait_for_event


START_TEST_BLOCK = 108


def test_massive_trxs(world_with_witnesses):
    logger.info(f'Start test_massive_trxs')

    # GIVEN
    world = world_with_witnesses
    alpha_witness_node = world.network('Alpha').node('WitnessNode0')
    beta_witness_node = world.network('Beta').node('WitnessNode0')
    node_under_test = world.network('Beta').node('NodeUnderTest')

    # WHEN
    run_networks(world)
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)

    # THEN

    wallets = [Wallet(attach_to=alpha_witness_node), Wallet(attach_to=beta_witness_node), Wallet(attach_to=node_under_test),
               Wallet(attach_to=alpha_witness_node), Wallet(attach_to=beta_witness_node), Wallet(attach_to=node_under_test),
               Wallet(attach_to=alpha_witness_node), Wallet(attach_to=beta_witness_node), Wallet(attach_to=node_under_test)]
    number_of_threads = len(wallets) * 10

    seed_account = Account("seed")
    for wallet in wallets:
        wallet.api.set_transaction_expiration(45*60)
        wallet.api.import_key(seed_account.private_key)

    accounts = ["alice", "bob", "carol"]
    account_count = len(accounts)

    for a in accounts:
        wallets[0].api.create_account_with_keys('initminer', a, '{}', seed_account.public_key, seed_account.public_key, seed_account.public_key, seed_account.public_key)
        wallets[0].api.transfer('initminer', a, Asset.Test(1000.0), 'initial cash')

    node_under_test.wait_number_of_blocks(2)

    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    event = Event()

    for i in range(number_of_threads):
        wallet = wallets[ i % len(wallets) ]
        def func(event, wallet, i):
            for j in itertools.count(start=1):
                for k in range(2000):
                    if event.is_set():
                        return
                    try:
                        wallet.api.transfer_nonblocking(accounts[k % account_count], accounts[(k+1) % account_count], Asset.Test(0.1), f'memo {i} {j} {k}')
                    except Exception:
                        pass

        executor.submit(func, event, wallet, i)

    logger.info('Starting main loop')
    try:
        while alpha_witness_node.get_number_of_forks()==0 and \
              beta_witness_node.get_number_of_forks()==0 and \
              node_under_test.get_number_of_forks()==0:
            #logger.info(f'{node_under_test} number_of_forks {node_under_test.get_number_of_forks()}')
            time.sleep(1)
    except KeyboardInterrupt:
        event.set()
        executor.shutdown(wait=True)
        raise
    logger.info(f'Detected fork')
    event.set()

    node_under_test.wait_number_of_blocks(10)
    executor.shutdown(wait=False)



def get_time_offset_from_file(name):
    timestamp = ''
    with open(name, 'r') as f:
        timestamp = f.read()
    timestamp = timestamp.strip()
    current_time = datetime.now(timezone.utc)
    new_time = datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=timezone.utc)
    difference = round(new_time.timestamp()-current_time.timestamp()) - 5 #reduce node start delay from 10s, caused test fails
    time_offset = str(difference) + 's'
    return time_offset


def run_networks(world, blocklog_directory=None, replay_all_nodes=True):
    if blocklog_directory is None:
        blocklog_directory = Path(__file__).parent.resolve()

    time_offset = get_time_offset_from_file(blocklog_directory/'timestamp')

    block_log = BlockLog(None, blocklog_directory/'block_log', include_index=False)

    logger.info('Running nodes...')

    nodes = world.nodes()
    nodes[0].run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)
    endpoint = nodes[0].get_p2p_endpoint()
    for node in nodes[1:]:
        node.config.p2p_seed_node.append(endpoint)
        if replay_all_nodes:
            node.run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)
        else:
            node.run(wait_for_live=False, time_offset=time_offset)

    for network in world.networks():
        network.is_running = True

    deadline = time.time() + 20
    for node in nodes:
        wait_for_event(
            node._Node__notifications.live_mode_entered_event,
            deadline=deadline,
            exception_message='Live mode not activated on time.'
        )