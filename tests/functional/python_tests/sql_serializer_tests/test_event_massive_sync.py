from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm.exc import MultipleResultsFound
import pytest
from pathlib import Path

from test_tools import logger


MASSIVE_SYNC_BLOCK_NUM = 100


@pytest.mark.parametrize("node_with_database", [Path().resolve()], indirect=True)
def test_event_massive_sync(node_with_database):
    logger.info(f'Start test_event_massive_sync')

    #GIVEN
    node_under_test, session, Base = node_with_database

    events_queue = Base.classes.events_queue

    # WHEN
    # TODO get_p2p_endpoint is workaround to check if replay is finished
    node_under_test.get_p2p_endpoint()

    # THEN
    logger.info(f'Checking that event MASSIVE_SYNC is in database')
    try:
        event = session.query(events_queue).filter(events_queue.event == 'MASSIVE_SYNC').one()
        assert event.block_num == MASSIVE_SYNC_BLOCK_NUM
        
    except MultipleResultsFound:
        logger.error(f'Multiple events MASSIVE_SYNC in database.')
        raise

    except NoResultFound:
        logger.error(f'Event MASSIVE_SYNC not in database.')
        raise
