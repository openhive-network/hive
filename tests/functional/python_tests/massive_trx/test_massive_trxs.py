from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import itertools
from pathlib import Path
import time
from threading import Event
import ujson

from test_tools import logger, Asset, Wallet, BlockLog, Account
from test_tools.private.wait_for import wait_for_event
from copy import deepcopy


START_TEST_BLOCK = 108


def translate_tx(tx):
    ops = tx['operations']
    tx['operations'] = []
    for op in ops:
        tx['operations'].append(dict(type=op[0]+'_operation',value=op[1]))

def test_massive_trxs(world_with_witnesses):
    logger.info(f'Start test_massive_trxs')

    # GIVEN
    world = world_with_witnesses
    some_node = world.network('Alpha').node('WitnessNode0')
    nodes = world.nodes()

    # WHEN
    run_networks(world)
    some_node.wait_for_block_with_number(START_TEST_BLOCK)

    # THEN
    PRODUCE = False # transaction production was made with edited sign_transaction code where tapos and expiration are not supplemented
                    # (otherwise we'd bind produced txs to "production reality" and we could not use them in "sending reality")
    if PRODUCE:
        wallets = [Wallet(attach_to=node) for node in nodes]
        number_of_threads = len(wallets) * 10

        for wallet in wallets:
            wallet.api.set_transaction_expiration(55*60)
            wallet.api.import_keys(['5KYaYMPMcqWGzaVQi8maqRXjtCh1PebdCCiYjK7sBZKeRvUzNpN','5JeDoeJZM9gxdbHQNroyUtb22kRppxvYHBZy481kWhHaSsWF6Us','5JFdNbspwzL4p7VbVCXApwXgJbWEbfjAsEiRL5ukQFnqFpFmQRF'])
        wallet = wallets[0]
    else:
        number_of_threads = len(nodes) * 10
        wallet = Wallet(attach_to=some_node)
        wallet.api.set_transaction_expiration(55*60)
        wallet.api.import_keys(['5KYaYMPMcqWGzaVQi8maqRXjtCh1PebdCCiYjK7sBZKeRvUzNpN','5JeDoeJZM9gxdbHQNroyUtb22kRppxvYHBZy481kWhHaSsWF6Us','5JFdNbspwzL4p7VbVCXApwXgJbWEbfjAsEiRL5ukQFnqFpFmQRF'])
    
    tx = dict(
        ref_block_num=105,
        ref_block_prefix=1721649276,
        expiration='2022-05-31T15:30:00',
        operations=[[
            'account_update',
            dict(
                account='initminer',
                posting=dict(
                    weight_threshold=3,
                    account_auths=[],
                    key_auths=[['TST7e8HejYNCqzMn68GX4UGVewxHGfTg6kP3xtm4mPrtw8yiePEmE',1],['TST71UPeJqwsPM1rk2M6VxKxheJduFYoffYcCfsEbuyhfRqRf2Et7',1],['TST63UnrDNpLMtUYSPFvxccEKnY3Xh3SnCHiv9Cnnv4uJDBQBD6ue',1]]
                ),
                memo_key='TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
                json_metadata=''
            )
        ]],
        extensions=[]
    )
    try:
        wallet.api.sign_transaction(tx)
    except Exception as e:
        logger.info(f'Exception in preparation phase: {str(e)}')
        return

    some_node.wait_number_of_blocks(2)
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    
    if PRODUCE:
        tx['operations'] = [[
            'custom_json',
            dict(
                required_auths=[],
                required_posting_auths=['initminer'],
                id='dummy',
                json='[[{"a":[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]}],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{"a":[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]}]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]'
            )
        ]]

        logger.info('Prepare transactions to send')
        for i in range(number_of_threads):
            wallet = wallets[ i % len(wallets) ]
            
            def func(w, i):
                thread_tx = deepcopy(tx)
                op = thread_tx['operations'][0][1]
                path = Path('./tx'+str(i)).absolute()
                logger.info(f'Thread {i} output: {path}')
                try:
                    with path.open('at') as output:
                        for j in range(180000//number_of_threads):
                            if (j % 100) == 0:
                                logger.info(f'Thread {i} preparing {j}')
                            op['id'] = 'cj'+str((j<<8)+i)
                            signed_tx = w.api.sign_transaction(thread_tx,broadcast=False)
                            signed_tx = ujson.dumps(signed_tx)
                            output.write(f'{signed_tx}\n')
                except Exception as e:
                    logger.info(f'Exception in thread {i} for iteration {j}: {str(e)}')
                logger.info(f'Thread {i} finished work')

            executor.submit(func, wallet, i)

        executor.shutdown(wait=True)
        logger.info('Prepare complete')
        return

    # since test uses custom_json on single account, it will only run when limit in witness plugin is turned off
    # (alternative would be to create tens of thousands of accounts and use custom_jsons on them to avoid limit,
    # but still there would be a chance of failure unless we had only 5 at max total per account)

    logger.info('Load transactions')
    path = Path('./transactions').absolute()
    transactions = []
    for i in range(number_of_threads):
        transactions.append(list())
    k = 0
    with path.open('rt') as input:
        for line in input:
            tx = ujson.loads(line)
            translate_tx(tx)
            transactions[k].append(tx)
            k = (k+1) % number_of_threads

    logger.info('Start sending transactions')
    event = Event()

    for i in range(number_of_threads):
        node = nodes[i % len(nodes)]

        def func(e, n, i):
            j = len(transactions[i])
            logger.info(f'Thread {i} has {j} to send')
            try:
                for tx in transactions[i]:
                    if e.is_set():
                        break
                    j -= 1
                    if (j % 100) == 0:
                        logger.info(f'Thread {i} has {j} to send')
                    n.api.network_broadcast.broadcast_transaction(trx=tx)
            except Exception as e:
                logger.info(f'Exception in thread {i} for transaction {tx}: {str(e)}')
            logger.info(f'Thread {i} finished work')

        executor.submit(func, event, node, i)

    logger.info('Starting main loop')
    try:
        while True:
            time.sleep(1)
            for n in nodes:
                if n.get_number_of_forks()!=0:
                    logger.info(f'Detected fork')
                    event.set()
                    break
    except KeyboardInterrupt:
        event.set()
        executor.shutdown(wait=True)
        raise

    event.set()
    executor.shutdown(wait=True)
    some_node.wait_number_of_blocks(20)



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


def run_networks(world, blocklog_directory=None):
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
        node.run(wait_for_live=False, replay_from=block_log, time_offset=time_offset)

    for network in world.networks():
        network.is_running = True

    deadline = time.time() + 20
    for node in nodes:
        wait_for_event(
            node._Node__notifications.live_mode_entered_event,
            deadline=deadline,
            exception_message='Live mode not activated on time.'
        )