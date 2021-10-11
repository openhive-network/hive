import pytest
from pathlib import Path

from test_tools import logger
from local_tools import get_head_block, get_irreversible_block


START_TEST_BLOCK = 101


@pytest.mark.parametrize("world_with_witnesses_and_database", [Path().resolve()], indirect=True)
def test_event_new_and_irreversible(world_with_witnesses_and_database):
    logger.info(f'Start test_event_new_and_irreversible')

    #GIVEN
    world, session, Base = world_with_witnesses_and_database
    node_under_test = world.network('Beta').node('NodeUnderTest')

    events_queue = Base.classes.events_queue

    # WHEN
    node_under_test.wait_for_block_with_number(START_TEST_BLOCK)
    previous_irreversible = get_irreversible_block(node_under_test)

    # THEN
    logger.info(f'Checking that event NEW_IRREVERSIBLE and NEW_BLOCK appear in database in correct order')
    for _ in range(20):
        node_under_test.wait_number_of_blocks(1)
        head_block = get_head_block(node_under_test)
        irreversible_block = get_irreversible_block(node_under_test)

        if irreversible_block > previous_irreversible:
            session.query(events_queue).\
                filter(events_queue.event == 'NEW_IRREVERSIBLE').\
                filter(events_queue.block_num == irreversible_block).\
                one()
            new_block_events = session.query(events_queue).\
                filter(events_queue.event == 'NEW_BLOCK').\
                all()
            assert new_block_events == []

            previous_irreversible = irreversible_block

        else:
            session.query(events_queue).\
                filter(events_queue.event == 'NEW_BLOCK').\
                filter(events_queue.block_num == head_block).\
                one()
