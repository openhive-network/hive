from test_tools import logger, Account, World, Asset, BlockLog
from local_tools import prepare_block_log, prepare_nodes, make_fork, wait_for_irreversible_progress
import sqlalchemy


FORKS = 1
BLOCKLOG_LENGTH = 195
START_TEST_BLOCK_NUMBER = 200


def test_undo_operations(world):
    #GIVEN
    block_log = prepare_block_log(world, BLOCKLOG_LENGTH)
    node_under_test = prepare_nodes(world, block_log, START_TEST_BLOCK_NUMBER)

    engine = sqlalchemy.create_engine('postgresql://myuser:mypassword@localhost/haf_block_log', echo=False)
    with engine.connect() as database_under_test:
        # WHEN
        fork_block = START_TEST_BLOCK_NUMBER
        logger.info(f'making fork at block {fork_block}')
        node_under_test.wait_for_block_with_number(fork_block)
        after_fork_block = make_fork(world)

        # THEN
        logger.info(f'waiting for progress of irreversible block')
        wait_for_irreversible_progress(node_under_test, after_fork_block)
        for block in range(fork_block, fork_block + after_fork_block):
            result = database_under_test.execute(f'select * from hive.operations where block_num={block}')
            for row in result:
                body = row['body']
                assert 'account_created_operation' not in body
