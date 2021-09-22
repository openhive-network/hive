import sqlalchemy

from test_tools import logger, BlockLog
from local_tools import prepare_database, prepare_nodes, make_fork, wait_for_irreversible_progress


START_TEST_BLOCK_NUMBER = 105


def test_undo_operations(world):
    #GIVEN
    engine = sqlalchemy.create_engine('postgresql://myuser:mypassword@localhost/haf_block_log', echo=False)
    prepare_database(engine)

    block_log = BlockLog(None, 'block_log', include_index=False)
    node_under_test = prepare_nodes(world, block_log, START_TEST_BLOCK_NUMBER)

    with engine.connect() as database_under_test:
        # WHEN
        fork_block = START_TEST_BLOCK_NUMBER
        logger.info(f'making fork at block {fork_block}')
        node_under_test.wait_for_block_with_number(fork_block)
        after_fork_block = make_fork(world)

        # THEN
        logger.info(f'waiting for progress of irreversible block')
        wait_for_irreversible_progress(node_under_test, after_fork_block)
        for block in range(fork_block, after_fork_block):
            logger.info(f"checking that operations in block {block} were reverted after switching fork")
            result = database_under_test.execute(f'select * from hive.operations where block_num={block}')
            for row in result:
                body = row['body']
                assert 'account_created_operation' not in body
