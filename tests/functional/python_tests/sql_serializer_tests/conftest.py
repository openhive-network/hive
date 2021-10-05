import pytest
from pathlib import Path
import sqlalchemy
from sqlalchemy_utils import database_exists, create_database, drop_database
from sqlalchemy.ext.automap import automap_base
from sqlalchemy.orm import sessionmaker
from contextlib import contextmanager
from shutil import copyfile

from test_tools import logger, World, BlockLog


@contextmanager
def session_scope(Session):
    """Provide a transactional scope around a series of operations."""
    session = Session()
    try:
        yield session
        session.commit()
    except:
        session.rollback()
        raise
    finally:
        session.close()


def get_timestamp_from_file(directory):
    timestamp = ''
    with open(directory/'timestamp', 'r') as f:
        timestamp = f.read()
    return timestamp.strip()


@pytest.fixture()
def witness_names():
    alpha_witness_names = [f'witness{i}-alpha' for i in range(12)]
    beta_witness_names = [f'witness{i}-beta' for i in range(8)]
    return alpha_witness_names, beta_witness_names


@pytest.fixture(scope="function")
def database():
    """
    Returns function that creates database with parametrized name and extension hive_fork_manager installed
    """

    def make_database(url):
        logger.info(f'Preparing fixture of database {url}')
        

        engine = sqlalchemy.create_engine(url, echo=False)
        if database_exists(engine.url):
            drop_database(engine.url)
        create_database(engine.url)
        with engine.connect() as connection:
            connection.execute('CREATE EXTENSION hive_fork_manager')

        with engine.connect() as connection:
            connection.execute('SET ROLE hived_group')

        return engine

    return make_database


@pytest.fixture(scope="function")
def world_with_witnesses_and_database(world, database, witness_names, request):
    logger.info(request.param)
    directory = request.param
    timestamp = get_timestamp_from_file(directory)
    world.set_timestamp(timestamp)
    import time
    time.sleep(.100)

    block_log = BlockLog(None, directory / 'block_log', include_index=False)
    engine = database('postgresql://myuser:mypassword@localhost/haf_block_log')

    alpha_witness_names, beta_witness_names = witness_names

    alpha_net = world.create_network('Alpha')
    alpha_witness_node = alpha_net.create_witness_node(witnesses=alpha_witness_names)
    beta_net = world.create_network('Beta')
    beta_witness_node = beta_net.create_witness_node(witnesses=beta_witness_names)
    node_under_test = beta_net.create_api_node(name = 'NodeUnderTest')

    # alpha_witness_node.config.enable_stale_production = True
    # alpha_witness_node.config.required_participation = 0
    # beta_witness_node.config.enable_stale_production = True
    # beta_witness_node.config.required_participation = 0

    node_under_test.config.plugin.append('sql_serializer')
    node_under_test.config.psql_url = str(engine.url)

    #RUN
    logger.info('Running nodes...')

    alpha_witness_node.run(wait_for_live=False, replay_from=block_log)
    endpoint = alpha_witness_node.get_p2p_endpoint()

    for node in [beta_witness_node, node_under_test]:
        node.config.p2p_seed_node.append(endpoint)
        node.run(wait_for_live=False, replay_from=block_log)

    alpha_net.is_running = True
    beta_net.is_running = True

    metadata = sqlalchemy.MetaData(schema="hive")
    Base = automap_base(bind=engine, metadata=metadata)
    Base.prepare(reflect=True)
    Session = sessionmaker(bind=engine)

    with session_scope(Session) as session:
        yield world, session, Base


@pytest.fixture(scope="function")
def node_with_database(world, database, request):
    logger.info(request.param)
    directory = request.param
    timestamp = get_timestamp_from_file(directory)
    world.set_timestamp(timestamp)

    block_log = BlockLog(None, directory / 'block_log', include_index=False)
    engine = database('postgresql://myuser:mypassword@localhost/haf_block_log')

    beta_net = world.create_network('Beta')
    node_under_test = beta_net.create_api_node(name = 'NodeUnderTest')

    node_under_test.config.plugin.append('sql_serializer')
    node_under_test.config.psql_url = str(engine.url)

    #RUN
    logger.info('Running node...')
    node_under_test.run(wait_for_live=False, replay_from=block_log)

    beta_net.is_running = True

    metadata = sqlalchemy.MetaData(schema="hive")
    Base = automap_base(bind=engine, metadata=metadata)
    Base.prepare(reflect=True)
    Session = sessionmaker(bind=engine)

    with session_scope(Session) as session:
        yield node_under_test, session, Base
