from typing import Optional

from test_tools.private.scope.scope_singleton import context
from test_tools.private.logger.logger_wrapper import LoggerWrapper


class LoggerInternalInterface:
    def __init__(self, name: str = '', instance: Optional['LoggerWrapper'] = None):
        self.__log_prefix = f'{name}: ' if name != '' else ''
        self.__instance: 'Optional[LoggerWrapper]' = instance

    @property
    def __logger(self) -> 'LoggerWrapper':
        return self.__instance if self.__instance is not None else context.get_logger()

    def debug(self, message: str):
        self.__logger.debug(f'{self.__log_prefix}{message}', stacklevel=1)

    def info(self, message: str):
        self.__logger.info(f'{self.__log_prefix}{message}', stacklevel=1)

    def create_child_logger(self, name: str) -> 'LoggerInternalInterface':
        logger_wrapper = LoggerWrapper(name, parent=self.__logger)
        return LoggerInternalInterface(name, logger_wrapper)


logger = LoggerInternalInterface()
