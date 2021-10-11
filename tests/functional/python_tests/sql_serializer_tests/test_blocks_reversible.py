import unittest
import pytest
from pathlib import Path

from test_tools import logger, BlockLog
from local_tools import wait_for_irreversible_progress, make_fork


START_TEST_BLOCK = 101


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_blocks_reversible(world_with_witnesses_and_database):
    logger.info(f'Start test_blocks_reversible')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    blocks_reversible = Base.classes.blocks_reversible

    # WHEN
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    logger.info(f'Making fork at block {START_TEST_BLOCK}')
    after_fork_block = make_fork(world)

    # THEN
    irreversible_block_num, head_block_number = wait_for_irreversible_progress(node_under_test, after_fork_block)
    case = unittest.TestCase()

    q = session.query(blocks_reversible).order_by(blocks_reversible.num).all()
    block_nums_reversible = [block.num for block in q]
    case.assertCountEqual(block_nums_reversible, range(irreversible_block_num, head_block_number+1))
