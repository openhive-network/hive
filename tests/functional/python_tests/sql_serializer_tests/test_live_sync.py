import unittest
from pathlib import Path

from test_tools import logger, Wallet, Asset
from local_tools import get_irreversible_block, wait_for_irreversible_progress, run_networks, Operation


START_TEST_BLOCK = 108


def test_live_sync(world_with_witnesses_and_database):
    logger.info(f'Start test_live_sync')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    witness_node = world.network('Alpha').node('WitnessNode0')
    node_under_test = world.network('Beta').node('NodeUnderTest')

    blocks = Base.classes.blocks
    transactions = Base.classes.transactions
    operations = Base.classes.operations

    # WHEN
    run_networks(world, Path().resolve())
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    wallet = Wallet(attach_to=witness_node)
    wallet.api.transfer('initminer', 'initminer', Asset.Test(1000), 'dummy transfer operation')
    transaction_block_num = START_TEST_BLOCK + 1

    # THEN
    wait_for_irreversible_progress(node_under_test, transaction_block_num)
    irreversible_block = get_irreversible_block(node_under_test)

    case = unittest.TestCase()
    blks = session.query(blocks).order_by(blocks.num).all()
    block_nums = [block.num for block in blks]
    case.assertCountEqual(block_nums, range(1, irreversible_block+1))

    session.query(transactions).filter(transactions.block_num == transaction_block_num).one()

    ops = session.query(operations).filter(operations.block_num == transaction_block_num).all()
    types = [Operation(raw_op=op).get_type() for op in ops]

    assert 'transfer_operation' in types
    assert 'producer_reward_operation' in types
