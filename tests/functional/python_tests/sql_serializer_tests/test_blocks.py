import unittest
import pytest
from pathlib import Path

from test_tools import logger, BlockLog
from local_tools import wait_for_irreversible_progress, make_fork, get_head_block, get_irreversible_block


START_TEST_BLOCK = 101


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_blocks(world_with_witnesses_and_database):
    logger.info(f'Start test_blocks_reversible')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    blocks_reversible = Base.classes.blocks_reversible

    fork_number = 0
    while True:
        logger.info(f'Fork number {fork_number}')

        # WHEN
        node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
        head_block = get_head_block(node_under_test)
        irreversible_block = get_irreversible_block(node_under_test)
        logger.info(f"Current head_block_number: {head_block}, irreversible_block_num: {irreversible_block}")
        after_fork_block = make_fork(world)

        # THEN
        irreversible_block_num, head_block_number = wait_for_irreversible_progress(node_under_test, after_fork_block)
        case = unittest.TestCase()

        q = session.query(blocks_reversible).order_by(blocks_reversible.num).all()
        block_nums_reversible = [block.num for block in q]
        try:
            case.assertCountEqual(block_nums_reversible, range(irreversible_block_num, head_block_number+1))
        except AssertionError:
            logger.info("Assertion about blocks_reversible failed")
            logger.info(f"actual: {block_nums_reversible}")
            logger.info(f"expected: {range(irreversible_block_num, head_block_number+1)}")

        fork_number = fork_number + 1
