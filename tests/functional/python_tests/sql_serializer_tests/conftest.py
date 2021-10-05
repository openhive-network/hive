import pytest
import sqlalchemy
from sqlalchemy_utils import database_exists, create_database, drop_database
from sqlalchemy.ext.automap import automap_base
from sqlalchemy.orm import sessionmaker, close_all_sessions
from sqlalchemy.pool import NullPool
import uuid

from test_tools import logger
from witnesses import alpha_witness_names, beta_witness_names


@pytest.fixture()
def witness_names():
    return alpha_witness_names, beta_witness_names


@pytest.fixture(scope="function")
def database():
    """
    Returns function that creates database with parametrized name and extension hive_fork_manager installed
    """

    def make_database(url):
        url = url + '_' + uuid.uuid4().hex
        logger.info(f'Preparing database {url}')
        if database_exists(url):
            drop_database(url)
        create_database(url)

        engine = sqlalchemy.create_engine(url, echo=False, poolclass=NullPool)
        with engine.connect() as connection:
            connection.execute('CREATE EXTENSION hive_fork_manager')

        with engine.connect() as connection:
            connection.execute('SET ROLE hived_group')

        Session = sessionmaker(bind=engine)
        session = Session()

        metadata = sqlalchemy.MetaData(schema="hive")
        Base = automap_base(bind=engine, metadata=metadata)
        Base.prepare(reflect=True)

        return session, Base

    yield make_database

    close_all_sessions()


@pytest.fixture(scope="function")
def world_with_witnesses_and_database(world, database, witness_names):
    alpha_witness_names, beta_witness_names = witness_names
    session, Base = database('postgresql://myuser:mypassword@localhost/haf_block_log')

    alpha_net = world.create_network('Alpha')
    alpha_net.create_witness_node(witnesses=alpha_witness_names)
    beta_net = world.create_network('Beta')
    beta_net.create_witness_node(witnesses=beta_witness_names)
    node_under_test = beta_net.create_api_node(name = 'NodeUnderTest')

    node_under_test.config.plugin.append('sql_serializer')
    node_under_test.config.psql_url = str(session.get_bind().url)

    yield world, session, Base
