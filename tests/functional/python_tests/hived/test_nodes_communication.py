
import pytest
import sqlalchemy
from sqlalchemy.ext.automap import automap_base
from sqlalchemy.pool import NullPool

from test_tools import BlockLog, constants, logger, Wallet
from sqlalchemy.orm import sessionmaker, close_all_sessions
from sqlalchemy_utils import database_exists, create_database, drop_database
from test_tools import paths_to_executables


# def test_data_propagation_between_nodes_via_p2p(world):
#     network = world.create_network()
#     init_node = network.create_init_node()
#     api_node = network.create_api_node()
#     network.run()

#     # Create account on init node
#     wallet = Wallet(attach_to=init_node)
#     wallet.api.create_account('initminer', 'alice', '{}')

#     # Check if account was send via P2P and is present on API nodevim
#     response = api_node.api.database.list_accounts(start='', limit=100, order='by_name')
#     assert 'alice' in [account['name'] for account in response['accounts']]


def test_massive_live_hived(world):
    network = world.create_network()
    init_node = network.create_init_node()
    init_node.run(replay_from="/home/dev/2fresh_block_logs/test_8000/block_log")
    # api_node = network.create_api_node()
    # network.create_api_node()
    # network.run()


    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    #
    # # Create account on init node
    wallet = Wallet(attach_to=init_node)
    #
    i = 9000
    while True:
        wallet.api.create_account('initminer', f'alice{i}', '{}')
        i += 1


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


def test_massive_live_haf_listening(database, world):
    paths_to_executables.set_path_of('hived', '/home/dev/1workspace/haf/build-testnet/hive/programs/hived/hived')
    paths_to_executables.set_path_of('cli_wallet', '/home/dev/1workspace/haf/build-testnet/hive/programs/cli_wallet/cli_wallet')
    paths_to_executables.set_path_of('get_dev_key', '/home/dev/1workspace/haf/build-testnet/hive/programs/util/get_dev_key')

    session, Base = database('postgresql:///haf_block_log_test')

    network = world.create_network()

    init_node = network.create_init_node()

    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    init_node.config.plugin.append('sql_serializer')
    init_node.config.psql_url = str(session.get_bind().url)

    init_node.run(
        replay_from=BlockLog(None, "/home/dev/2fresh_block_logs/test_100/block_log", include_index=False),
        stop_at_block=3222,
        wait_for_live=False,
    )

    while True:
        pass



def test_massive_live_haf_mining(database, world):
    paths_to_executables.set_path_of('hived', '/home/dev/1workspace/haf/build-testnet/hive/programs/hived/hived')
    paths_to_executables.set_path_of('cli_wallet', '/home/dev/1workspace/haf/build-testnet/hive/programs/cli_wallet/cli_wallet')
    paths_to_executables.set_path_of('get_dev_key', '/home/dev/1workspace/haf/build-testnet/hive/programs/util/get_dev_key')

    session, Base = database('postgresql://haf_admin:devdevdev@localhost:5432/haf_block_log_test')

    network = world.create_network()

    init_node = network.create_init_node()

    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    init_node.config.plugin.append('sql_serializer')
    init_node.config.psql_url = str(session.get_bind().url)

    init_node.run(replay_from="/home/dev/2fresh_block_logs/test_8000/block_log", wait_for_live=False)

    wallet = Wallet(attach_to=init_node)

    i = 9000
    while True:
        wallet.api.create_account('initminer', f'alice{i}', '{}')
        i += 1
