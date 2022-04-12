from concurrent.futures import ThreadPoolExecutor
import datetime
import os
import time

import pytest

from test_tools import Account, Asset, BlockLog, logger, Wallet

BLOCK_LOG, BLOCK_LOG_LENGTH = {
    'long': [
        BlockLog(None, '/home/dev/blocks/5M/mirrornet/block_log', include_index=False),
        5_000_000,
    ],
}['long']

SHARED_FILE_SIZE = '1G'
SKELETON_KEY = '5JhCDhQK2GaShbTnWWHXH2k5jtZi4TqotihFzWo4doJvbxnmVnc'
CHAIN_ID = 42


@pytest.fixture(autouse=True)
def set_path_of_fake_time_lib():
    os.environ.update(LIBFAKETIME_PATH='/home/dev/Desktop/libfaketime/src/libfaketime.so.1')


def test_mirrornet_replay_and_synchronization(world):
    logger.info('Starting replay of first ApiNode...')

    replayed_node = world.create_api_node()
    replayed_node.config.shared_file_size = SHARED_FILE_SIZE
    replayed_node.run(
        replay_from=BLOCK_LOG,
        arguments=['--chain-id=42', '--skeleton-key=5JhCDhQK2GaShbTnWWHXH2k5jtZi4TqotihFzWo4doJvbxnmVnc'],
        wait_for_live=False,
    )

    logger.info('Replay completed, syncing second ApiNode with the first one...')

    synced_node = world.create_api_node()
    synced_node.config.p2p_seed_node = replayed_node.get_p2p_endpoint()
    synced_node.config.shared_file_size = SHARED_FILE_SIZE
    synced_node.run(
        arguments=['--chain-id=42', '--skeleton-key=5JhCDhQK2GaShbTnWWHXH2k5jtZi4TqotihFzWo4doJvbxnmVnc'],
        wait_for_live=False,
    )

    while (last_block := synced_node.get_last_block_number()) < BLOCK_LOG_LENGTH:
        logger.info(f'Second ApiNode is syncing, {last_block=}...')
        time.sleep(5)

    assert replayed_node.get_last_block_number() == BLOCK_LOG_LENGTH
    assert synced_node.get_last_block_number() == BLOCK_LOG_LENGTH

    logger.info(f'Syncing completed, both nodes have head at {BLOCK_LOG_LENGTH} block.')
    logger.info('Entering infinite loop...')

    while True:
        pass


def test_continuation_of_block_production_after_replay(world):
    witnesses = ['gtg', 'blocktrades', 'ausbitbank', 'good-karma', 'roelandp', 'steempeak', 'themarkymark',
                 'therealwolf', 'yabapmatt', 'ocd-witness', 'abit', 'stoodkev', 'pharesim', 'steempress', 'emrebeyler',
                 'followbtcnews', 'anyx', 'deathwing', 'smooth.witness', 'quochoy', 'someguy123', 'guiltyparties',
                 'threespeak', 'engrave', 'mahdiyari']

    additional_witnesses = ['roadscape', 'clayop', 'gxt-1080-sc-0003', 'joseph', 'jesta', 'complexring', 'xeldal',
                            'delegate.lafona', 'riverhead', 'steemychicken1', 'steve-walschot', 'arhag', 'steemed',
                            'boatymcboatface', 'ihashfury', 'datasecuritynode', 'wackou']

    witness_node = world.create_witness_node(witnesses=[*witnesses, *additional_witnesses])
    witness_node.config.shared_file_size = SHARED_FILE_SIZE
    witness_node.config.p2p_seed_node = '0.0.0.0:12345'
    witness_node.config.private_key = SKELETON_KEY
    witness_node.run(
        replay_from=BLOCK_LOG,
        arguments=[f'--chain-id={CHAIN_ID}', f'--skeleton-key={SKELETON_KEY}'],
        time_offset="@2016-09-15 19:47:24",
    )

    while True:
        last_block = witness_node.get_last_block_number()
        logger.info(f'{witness_node} produces blocks, {last_block=}...')
        time.sleep(3)


# env: FAKETIME=@2016-09-15 19:47:24;FAKETIME_DONT_RESET=1;LD_PRELOAD=/home/dev/Desktop/libfaketime/src/libfaketime.so.1
def test_network_setup(world):
    from test_tools.constants import WorldCleanUpPolicy
    world.set_clean_up_policy(WorldCleanUpPolicy.DO_NOT_REMOVE_FILES)

    witnesses = ['roadscape', 'arhag', 'smooth.witness', 'xeldal', 'complexring', 'riverhead', 'clayop', 'blocktrades',
                 'abit', 'wackou', 'steemed', 'joseph', 'jesta', 'pharesim', 'datasecuritynode', 'boatymcboatface',
                 'steemychicken1', 'ihashfury', 'delegate.lafona', 'au1nethyb1', 'witness.svk', 'silversteem']

    # Count time offset
    t0 = datetime.datetime.strptime('@2016-09-15 19:47:21', '@%Y-%m-%d %H:%M:%S').replace(tzinfo=datetime.timezone.utc)
    diff = datetime.datetime.now(datetime.timezone.utc) - t0
    time_offset = f'-{int(diff.total_seconds())}'

    init_node = world.create_init_node()
    init_node.config.witness = witnesses
    init_node.config.shared_file_size = SHARED_FILE_SIZE
    init_node.config.p2p_seed_node = '0.0.0.0:12345'  # Non-existing address works as no p2p seed nodes
    init_node.config.private_key = SKELETON_KEY
    init_node.config.plugin.remove('account_history_rocksdb')  # To speed up replay
    init_node.config.plugin.remove('account_history_api')  # To speed up replay
    logger.info('Running InitNode...')
    init_node.run(
        replay_from=BLOCK_LOG,
        arguments=[f'--chain-id={CHAIN_ID}', f'--skeleton-key={SKELETON_KEY}'],
        time_offset=time_offset,
    )

    logger.info('InitNode is replayed and produce blocks')

    witnesses_group_0 = witnesses[:10]
    witness_node_0 = world.create_witness_node(witnesses=witnesses_group_0)
    witness_node_0.config.shared_file_size = SHARED_FILE_SIZE
    witness_node_0.config.p2p_seed_node = init_node.get_p2p_endpoint()
    witness_node_0.config.private_key = [Account(witness).private_key for witness in witnesses_group_0]
    witness_node_0.config.plugin.remove('account_history_rocksdb')  # To speed up replay
    witness_node_0.config.plugin.remove('account_history_api')  # To speed up replay
    logger.info('Replaying WitnessNode0...')
    witness_node_0.run(
        replay_from=BLOCK_LOG,
        arguments=[f'--chain-id={CHAIN_ID}', f'--skeleton-key={SKELETON_KEY}'],
        exit_before_synchronization=True,
    )

    witnesses_group_1 = witnesses[10:]
    witness_node_1 = world.create_witness_node(witnesses=witnesses_group_1)
    witness_node_1.config.shared_file_size = SHARED_FILE_SIZE
    witness_node_1.config.p2p_seed_node = init_node.get_p2p_endpoint()
    witness_node_1.config.private_key = [Account(witness).private_key for witness in witnesses_group_1]
    witness_node_1.config.plugin.remove('account_history_rocksdb')  # To speed up replay
    witness_node_1.config.plugin.remove('account_history_api')  # To speed up replay
    logger.info('Replaying WitnessNode1...')
    witness_node_1.run(
        replay_from=BLOCK_LOG,
        arguments=[f'--chain-id={CHAIN_ID}', f'--skeleton-key={SKELETON_KEY}'],
        exit_before_synchronization=True,
    )

    logger.info('================= After replays =================')

    witness_node_0.run(
        arguments=[f'--chain-id={CHAIN_ID}', f'--skeleton-key={SKELETON_KEY}'],
        time_offset=time_offset,
    )
    logger.info('witness_node_0 is run!')
    witness_node_1.config.p2p_seed_node = witness_node_0.get_p2p_endpoint()
    witness_node_1.run(
        arguments=[f'--chain-id={CHAIN_ID}', f'--skeleton-key={SKELETON_KEY}'],
        time_offset=time_offset,
    )
    logger.info('witness_node_1 is run!')

    logger.info('Deactivate witnesses on init node and activate them on witness nodes...')
    wallet = Wallet(attach_to=witness_node_0, additional_arguments=[f'--chain-id={CHAIN_ID}'])
    wallet.api.import_key(SKELETON_KEY)

    for witness in witnesses:
        wallet.api.update_witness(
            witness, 'url', Account(witness).public_key,
            {"account_creation_fee": Asset.Hive(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
        )

    wallet.close()

    logger.info('Closing InitNode, because all network is set up and is no longer needed.')
    init_node.close()

    while True:
        pass
