import unittest
import pytest
from pathlib import Path

from test_tools import logger, Wallet, Asset
from local_tools import wait_for_irreversible_progress, get_irreversible_block


START_TEST_BLOCK = 101


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_live_sync(world_with_witnesses_and_database):
    logger.info(f'Start test_live_sync')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    witness_node = world.network('Beta').node('WitnessNode0')
    wallet = Wallet(attach_to=witness_node)
    node_under_test = world.network('Beta').node('NodeUnderTest')

    blocks = Base.classes.blocks
    transactions = Base.classes.transactions
    operations = Base.classes.operations

    # WHEN
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    wallet.api.transfer('initminer', 'initminer', Asset.Test(1000), 'dummy transfer operation')
    transaction_block_num = START_TEST_BLOCK + 1

    # THEN
    wait_for_irreversible_progress(node_under_test, transaction_block_num)
    irreversible_block = get_irreversible_block(node_under_test)

    case = unittest.TestCase()
    q = session.query(blocks).order_by(blocks.num).all()
    block_nums = [block.num for block in q]
    case.assertCountEqual(block_nums, range(1, irreversible_block+1))

    q = session.query(transactions).filter(transactions.block_num == transaction_block_num).one()

    q = session.query(operations).filter(operations.block_num == transaction_block_num).order_by(operations.op_type_id).all()
    bodies = [row.body for row in q]
    assert 'transfer_operation' in bodies[0]
    assert 'producer_reward_operation' in bodies[1]
