import logging
import pytest

from test_tools import constants, World
from test_tools.private.scope import context


def pytest_addoption(parser):
    parser.addoption("--http-endpoint", action="store", default='0.0.0.0:18091', type=str,
                     help='specifies http_endpoint of reference node')
    parser.addoption("--ws-endpoint", action="store", default='0.0.0.0:18090', type=str,
                     help='specifies ws_endpoint of reference node')
    parser.addoption("--wallet-path", action="store", default='/home/dev/ComparationHF25HF26Mainnet/src/hive_HF26'
                                                              '/build/programs/cli_wallet/cli_wallet',
                     type=str, help='specifies path of reference cli wallet')


@pytest.fixture
def world():
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger('urllib3.connectionpool').propagate = False
