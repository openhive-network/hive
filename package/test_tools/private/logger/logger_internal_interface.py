from typing import Optional

from test_tools.private.logger.logger_interface_base import LoggerInterfaceBase
from test_tools.private.logger.logger_wrapper import LoggerWrapper


class LoggerInternalInterface(LoggerInterfaceBase):
    def __init__(self, name: str = '', instance: Optional['LoggerWrapper'] = None):
        super().__init__(instance, message_prefix=f'{name}: ' if name != '' else '')

    def create_child_logger(self, name: str) -> 'LoggerInternalInterface':
        logger_wrapper = LoggerWrapper(name, parent=self._logger)
        return LoggerInternalInterface(name, logger_wrapper)


logger = LoggerInternalInterface()
