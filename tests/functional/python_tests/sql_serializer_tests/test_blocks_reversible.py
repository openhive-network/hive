import unittest
from pathlib import Path

from test_tools import logger, Asset
from local_tools import make_fork, wait_for_irreversible_progress, run_networks, Operation


START_TEST_BLOCK = 108


def test_blocks_reversible(world_with_witnesses_and_database):
    logger.info(f'Start test_blocks_reversible')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    blocks_reversible = Base.classes.blocks_reversible

    # WHEN
    run_networks(world, Path().resolve())
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    after_fork_block = make_fork(
        world,
        main_chain_ops = [Operation('transfer', 'initminer', 'initminer', Asset.Test(1234), 'main chain transfer operation')],
        fork_chain_ops = [Operation('transfer_to_vesting', 'initminer', 'initminer', Asset.Test(1234))],
    )

    # THEN
    irreversible_block_num, head_block_number = wait_for_irreversible_progress(node_under_test, after_fork_block+1)
    case = unittest.TestCase()

    blks = session.query(blocks_reversible).order_by(blocks_reversible.num).all()
    block_nums_reversible = [block.num for block in blks]
    case.assertCountEqual(block_nums_reversible, range(irreversible_block_num, head_block_number+1))
