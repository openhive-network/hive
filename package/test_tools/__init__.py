def __create_logger():
    from test_tools.private.logger import Logger
    return Logger()

logger = __create_logger()

from test_tools.private.asset import Asset
from test_tools.private.block_log import BlockLog
from test_tools.account import Account
from test_tools.world import World
