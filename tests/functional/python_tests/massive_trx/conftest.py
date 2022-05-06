import pytest
import sqlalchemy
from sqlalchemy_utils import database_exists, create_database, drop_database
from sqlalchemy.ext.automap import automap_base
from sqlalchemy.orm import sessionmaker, close_all_sessions
from sqlalchemy.pool import NullPool
from uuid import uuid4

from test_tools import logger, constants, World
from test_tools.private.scope import context
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import

from witnesses import alpha_witness_names, beta_witness_names


def pytest_exception_interact(report):
    logger.error(f'Test exception:\n{report.longreprtext}')


@pytest.fixture(scope="function")
def world():
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world


@pytest.fixture()
def witness_names():
    return alpha_witness_names, beta_witness_names


@pytest.fixture(scope="function")
def world_with_witnesses(world, witness_names):
    alpha_witness_names, beta_witness_names = witness_names

    alpha_net = world.create_network('Alpha')
    alpha_net.create_witness_node(witnesses=alpha_witness_names)
    beta_net = world.create_network('Beta')
    beta_net.create_witness_node(witnesses=beta_witness_names)
    node_under_test = beta_net.create_api_node(name = 'NodeUnderTest')

    for node in world.nodes():
        node.config.log_logger = '{"name":"default","level":"debug","appender":"stderr,p2p"} '\
                                 '{"name":"user","level":"debug","appender":"stderr,p2p"} '\
                                 '{"name":"chainlock","level":"debug","appender":"p2p"} '\
                                 '{"name":"sync","level":"debug","appender":"p2p"} '\
                                 '{"name":"p2p","level":"debug","appender":"p2p"}'

    yield world
