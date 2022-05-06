import os
import time

import pytest

from test_tools import BlockLog, logger

from .local_tools import Mirrornet


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
        time_offset='@2016-09-15 19:47:24',
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

    mirrornet = Mirrornet(world, '/home/dev/blocks/5M/mirrornet/block_log', '@2016-09-15 19:47:24')

    init_node = mirrornet.create_and_run_init_node(witnesses=witnesses)

    witness_node_0 = mirrornet.create_witness_node(witnesses=witnesses[:10])
    witness_node_1 = mirrornet.create_witness_node(witnesses=witnesses[10:])

    mirrornet.run_witness_nodes(witness_node_0, witness_node_1)

    logger.info('Closing InitNode, because all network is set up and is no longer needed.')
    init_node.close()

    while True:
        pass
