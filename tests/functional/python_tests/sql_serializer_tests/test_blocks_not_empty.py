import unittest
import pytest
from pathlib import Path

from test_tools import logger, BlockLog
from local_tools import wait_for_irreversible_progress, make_fork


START_TEST_BLOCK = 105


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_blocks_not_empty(world_with_witnesses_and_database):
    logger.info(f'Start test_blocks_not_empty')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    blocks = Base.classes.blocks
    blocks_reversible = Base.classes.blocks_reversible

    # WHEN
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    logger.info(f'Making fork at block {START_TEST_BLOCK}')
    after_fork_block = make_fork(world)

    # THEN
    irreversible_block_num, head_block_number = wait_for_irreversible_progress(node_under_test, after_fork_block)
    case = unittest.TestCase()

    q = session.query(blocks).order_by(blocks.num).all()
    block_nums = [block.num for block in q]
    case.assertCountEqual([i for i in block_nums], [i for i in range(1, irreversible_block_num+1)])

    q = session.query(blocks_reversible).order_by(blocks_reversible.num).all()
    block_nums_reversible = [block.num for block in q]
    case.assertCountEqual(block_nums_reversible, range(irreversible_block_num, head_block_number+1))

    

    # assert trx == []

    # #GIVEN
    # world, engine = world_with_witnesses_and_database
    # node_under_test = world.network('Beta').node('NodeUnderTest')

    # with engine.connect() as database_under_test:
    #     # WHEN
    #     node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    #     logger.info(f'Making fork at block {START_TEST_BLOCK}')
    #     make_fork(world)

    #     # THEN
    #     irreversible_block_num = node_under_test.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    #     logger.info(f'last_irreversible_block_num: {irreversible_block_num}')
    #     result = database_under_test.execute('SELECT * FROM hive.blocks ORDER BY num')
    #     blocks = set()
    #     for row in result:
    #         blocks.add(row['num'])
    #     logger.info(f'blocks: {blocks}')

    #     head_block_num = node_under_test.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    #     logger.info(f'head_block_number: {head_block_num}')
    #     result = database_under_test.execute('SELECT * FROM hive.blocks_reversible ORDER BY num')
    #     blocks_reversible = set()
    #     for row in result:
    #         blocks_reversible.add(row['num'])
    #     logger.info(f'blocks_reversible: {blocks_reversible}')

    #     # assert table hive.blocks contains blocks 1..irreversible_block_num
    #     for i in range(1, irreversible_block_num+1):
    #         assert i in blocks
    #     # assert table hive.blocks contains blocks irreversible_block_num+1..head_block_number
    #     for i in range(irreversible_block_num+1, head_block_num+1):
    #         assert i in blocks_reversible
