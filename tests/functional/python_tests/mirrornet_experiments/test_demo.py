import os

import pytest

from test_tools import logger

from .local_tools import Mirrornet


@pytest.fixture(autouse=True)
def set_path_of_fake_time_lib():
    os.environ.update(LIBFAKETIME_PATH='/home/dev/Desktop/libfaketime/src/libfaketime.so.1')


def test_network_setup(world):
    witnesses = ['roadscape', 'arhag', 'smooth.witness', 'xeldal', 'complexring', 'riverhead', 'clayop', 'blocktrades',
                 'abit', 'wackou', 'steemed', 'joseph', 'jesta', 'pharesim', 'datasecuritynode', 'boatymcboatface',
                 'steemychicken1', 'ihashfury', 'delegate.lafona', 'au1nethyb1', 'witness.svk', 'silversteem']

    mirrornet = Mirrornet(world, '/home/dev/blocks/5M/mirrornet/block_log', '@2016-09-15 19:47:24')

    init_node = mirrornet.create_and_run_init_node(witnesses=witnesses)

    witness_nodes = [
        mirrornet.create_witness_node(witnesses=witnesses[:5]),
        mirrornet.create_witness_node(witnesses=witnesses[5:10]),
        mirrornet.create_witness_node(witnesses=witnesses[10:15]),
        mirrornet.create_witness_node(witnesses=witnesses[15:]),
    ]
    mirrornet.run_witness_nodes(*witness_nodes)

    logger.info('Closing InitNode, because all network is set up and is no longer needed.')
    init_node.close()

    logger.info('Preparation completed successfully. Entering infinite loop...')
    while True:
        pass
