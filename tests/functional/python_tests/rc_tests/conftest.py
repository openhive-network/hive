import pytest
from uuid import uuid4

from test_tools import logger, World
from test_tools.private.scope import context
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import

alpha_witness_names = [f'witness{i}-alpha' for i in range(12)]
beta_witness_names = [f'witness{i}-beta' for i in range(8)]


def pytest_exception_interact(report):
    logger.error(f'Test exception:\n{report.longreprtext}')


@pytest.fixture(scope="function")
def world():
    with World(directory=context.get_current_directory()) as world:
        yield world


@pytest.fixture()
def witness_names():
    return alpha_witness_names, beta_witness_names


@pytest.fixture(scope="function")
def world_with_witnesses_and_database(world, witness_names):
    alpha_witness_names, beta_witness_names = witness_names

    alpha_net = world.create_network('Alpha')
    alpha_net.create_witness_node(witnesses=alpha_witness_names)
    beta_net = world.create_network('Beta')
    beta_net.create_witness_node(witnesses=beta_witness_names)

    for node in world.nodes():
        node.config.log_logger = '{"name":"default","level":"info","appender":"stderr"} '\
                                 '{"name":"user","level":"debug","appender":"stderr"} '\
                                 '{"name":"p2p","level":"debug","appender":"p2p"}'

    yield world

