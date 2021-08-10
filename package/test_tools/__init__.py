# pylint: disable=import-outside-toplevel, wrong-import-position
# This is required because of bad implementation of logger.
# Hopefully it will be rewritten in near future and then above
# pylint-ignore should be removed.

def __create_logger():
    from test_tools.private.logger import Logger
    return Logger()

logger = __create_logger()

from test_tools.private.asset import Asset
from test_tools.private.block_log import BlockLog
from test_tools.private.main_net import main_net_singleton as main_net
from test_tools.account import Account
from test_tools.world import World
