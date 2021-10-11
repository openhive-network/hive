import pytest
from pathlib import Path

from test_tools import logger, BlockLog
from local_tools import make_fork, wait_for_irreversible_progress


START_TEST_BLOCK = 101


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_undo_transactions(world_with_witnesses_and_database):
    logger.info(f'Start test_undo_transactions')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    transactions = Base.classes.transactions

    # WHEN
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    logger.info(f'Making fork at block {START_TEST_BLOCK}')
    after_fork_block = make_fork(world)

    # THEN
    wait_for_irreversible_progress(node_under_test, after_fork_block)
    trx = session.query(transactions).filter(transactions.block_num > START_TEST_BLOCK).all()
    
    assert trx == []
