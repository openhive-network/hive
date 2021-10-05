from pathlib import Path

from test_tools import logger, Asset
from local_tools import make_fork, wait_for_irreversible_progress, run_networks, Operation


START_TEST_BLOCK = 108


def test_operations_after_switchng_fork(world_with_witnesses_and_database):
    logger.info(f'Start test_operations_after_switchng_fork')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    transactions = Base.classes.transactions
    operations = Base.classes.operations

    # WHEN
    run_networks(world, Path().resolve())
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    after_fork_block = make_fork(
        world,
        main_chain_ops = [Operation('transfer', 'initminer', 'initminer', Asset.Test(1234), 'main chain transfer operation')],
        fork_chain_ops = [Operation('transfer_to_vesting', 'initminer', 'initminer', Asset.Test(1234))],
    )

    # THEN
    wait_for_irreversible_progress(node_under_test, after_fork_block)
    trx = session.query(transactions).filter(transactions.block_num > START_TEST_BLOCK).one()

    ops = session.query(operations).filter(operations.block_num == trx.block_num).all()
    types = [Operation(raw_op=op).get_type() for op in ops]

    assert 'producer_reward_operation' in types
    assert 'transfer_operation' in types
    assert 'transfer_to_vesting_operation' not in types
