from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm.exc import MultipleResultsFound
from pathlib import Path

from test_tools import logger, Asset, Wallet
from local_tools import make_fork, wait_for_irreversible_progress, run_networks, Operation


START_TEST_BLOCK = 108


def test_debug_set_witness_schedule(world_with_witnesses_and_database):
    logger.info(f'Start test_debug_set_witness_schedule')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    witness_node = world.network('Alpha').node('WitnessNode0')
    node_under_test = world.network('Beta').node('NodeUnderTest')

    operations = Base.classes.operations

    # WHEN
    run_networks(world, Path().resolve())
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    wallet = Wallet(attach_to=witness_node)
    wallet.api.debug_set_witness_schedule(['witness1-alpha', 'witness3-alpha', 'witness5-alpha'])
    transaction_block_num = START_TEST_BLOCK + 1

    # THEN
    wait_for_irreversible_progress(node_under_test, 20000)
