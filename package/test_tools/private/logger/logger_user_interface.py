from typing import Optional, TYPE_CHECKING

from test_tools.private.logger.logger_interface_base import LoggerInterfaceBase

if TYPE_CHECKING:
    from test_tools.private.logger.logger_wrapper import LoggerWrapper


class LoggerUserInterface(LoggerInterfaceBase):
    def __init__(self, instance: Optional['LoggerWrapper'] = None):
        super().__init__(instance, message_prefix='')


logger = LoggerUserInterface()
