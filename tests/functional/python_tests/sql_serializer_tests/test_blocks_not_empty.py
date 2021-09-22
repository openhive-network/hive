import sqlalchemy

from test_tools import logger, BlockLog
from local_tools import prepare_database, prepare_nodes, make_fork, wait_for_irreversible_progress


START_TEST_BLOCK_NUMBER = 105

def test_blocks_not_empty(world):
    #GIVEN
    engine = sqlalchemy.create_engine('postgresql://myuser:mypassword@localhost/haf_block_log', echo=False)
    prepare_database(engine)

    block_log = BlockLog(None, 'block_log', include_index=False)
    node_under_test = prepare_nodes(world, block_log, START_TEST_BLOCK_NUMBER)

    engine = sqlalchemy.create_engine('postgresql://myuser:mypassword@localhost/haf_block_log', echo=False)
    with engine.connect() as database_under_test:
        # WHEN
        fork_block = START_TEST_BLOCK_NUMBER
        logger.info(f'making fork at block {fork_block}')
        node_under_test.wait_for_block_with_number(fork_block)
        fork_block = make_fork(world)

        # THEN
        irreversible_block_num = node_under_test.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
        logger.info(f'last_irreversible_block_num: {irreversible_block_num}')
        result = database_under_test.execute('select * from hive.blocks order by num')
        blocks = set()
        for row in result:
            blocks.add(row['num'])
        logger.info(f'blocks: {blocks}')

        head_block_num = node_under_test.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
        logger.info(f'head_block_number: {head_block_num}')
        result = database_under_test.execute('select * from hive.blocks_reversible order by num')
        blocks_reversible = set()
        for row in result:
            blocks_reversible.add(row['num'])
        logger.info(f'blocks_reversible: {blocks_reversible}')

        # assert table hive.blocks contains blocks 1..irreversible_block_num
        for i in range(1, irreversible_block_num+1):
            assert i in blocks
        # assert table hive.blocks contains blocks irreversible_block_num+1..head_block_number
        for i in range(irreversible_block_num+1, head_block_num+1):
            assert i in blocks_reversible
