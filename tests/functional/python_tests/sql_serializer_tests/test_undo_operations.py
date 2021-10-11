from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm.exc import MultipleResultsFound
import pytest
from pathlib import Path

from test_tools import logger
from local_tools import make_fork, wait_for_irreversible_progress


START_TEST_BLOCK = 101


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_undo_operations(world_with_witnesses_and_database):
    logger.info(f'Start test_undo_operations')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    operations = Base.classes.operations

    # WHEN
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    logger.info(f'Making fork at block {START_TEST_BLOCK}')
    fork_block = START_TEST_BLOCK
    after_fork_block = make_fork(world)

    # THEN
    wait_for_irreversible_progress(node_under_test, after_fork_block)
    for i in range(fork_block, after_fork_block):
        try:
            # there should be exactly one producer_reward_operation
            session.query(operations).filter(operations.block_num == i).one()
        
        except MultipleResultsFound:
            logger.error(f'Multiple operations in block {i}.')
            raise
        except NoResultFound:
            logger.error(f'No producer_reward_operation in block {i}.')
            raise
