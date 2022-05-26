import os

import pytest
import sqlalchemy
from sqlalchemy.ext.automap import automap_base
from sqlalchemy.pool import NullPool
import pytest
from sqlalchemy.orm import close_all_sessions, sessionmaker
from sqlalchemy_utils import create_database, database_exists, drop_database
from test_tools import (BlockLog, Wallet, constants, logger,
                        paths_to_executables)

from .local_tools import Mirrornet


@pytest.fixture(scope="function")
def database():
    """
    Returns factory function that creates database with parametrized name and extension hive_fork_manager installed
    """

    def make_database(url):
        logger.info(f'Preparing database {url}')
        if database_exists(url):
            drop_database(url)
        create_database(url)

        engine = sqlalchemy.create_engine(url, echo=False, poolclass=NullPool)
        with engine.connect() as connection:
            connection.execute('CREATE EXTENSION hive_fork_manager CASCADE;')

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


@pytest.fixture(autouse=True)
def set_path_of_fake_time_lib():
    os.environ.update(LIBFAKETIME_PATH='/home/dev/1workspace/libfaketime/src/libfaketime.so.1')


def test_network_setup(world):
    witnesses = ['roadscape', 'arhag', 'smooth.witness', 'xeldal', 'complexring', 'riverhead', 'clayop', 'blocktrades',
                 'abit', 'wackou', 'steemed', 'joseph', 'jesta', 'pharesim', 'datasecuritynode', 'boatymcboatface',
                 'steemychicken1', 'ihashfury', 'delegate.lafona', 'au1nethyb1', 'witness.svk', 'silversteem']

    mirrornet = Mirrornet(world, '/home/dev/1workspace/1nodes/haf_mirrornet_5m/blockchain/block_log', '@2016-09-15 19:47:24')

    init_node = mirrornet.create_and_run_init_node(witnesses=witnesses)

    logger.info('Preparation completed successfully. Entering infinite loop...')
    while True:
        pass


def test_massive_live_haf_mining(database, world):
    paths_to_executables.set_path_of('hived', '/home/dev/1workspace/haf/build-mirrornet/hive/programs/hived/hived')
    paths_to_executables.set_path_of('cli_wallet', '/home/dev/1workspace/haf/build-mirrornet/hive/programs/cli_wallet/cli_wallet')
    paths_to_executables.set_path_of('get_dev_key', '/home/dev/1workspace/haf/build-mirrornet/hive/programs/util/get_dev_key')

    session, Base = database('postgresql:///haf_block_log_test')

    witnesses = ['roadscape', 'arhag', 'smooth.witness', 'xeldal', 'complexring', 'riverhead', 'clayop', 'blocktrades',
                 'abit', 'wackou', 'steemed', 'joseph', 'jesta', 'pharesim', 'datasecuritynode', 'boatymcboatface',
                 'steemychicken1', 'ihashfury', 'delegate.lafona', 'au1nethyb1', 'witness.svk', 'silversteem']


    mirrornet = Mirrornet(world, '/home/dev/1workspace/1nodes/haf_mirrornet_5m/blockchain/block_log', '@2016-09-15 19:47:24')
 
    init_node = mirrornet.create_init_node(witnesses=witnesses)

    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    init_node.config.plugin.append('sql_serializer')
    init_node.config.psql_url = str(session.get_bind().url)

    mirrornet.run_init_node()

    wallet = Wallet(attach_to=init_node)

    i = 10000
    while True:
        wallet.api.create_account('initminer', f'alice{i}', '{}')
        i += 1
