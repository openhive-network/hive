def __create_logger():
    from .private.logger import Logger
    return Logger()

logger = __create_logger()

from .account import Account
from .world import World
