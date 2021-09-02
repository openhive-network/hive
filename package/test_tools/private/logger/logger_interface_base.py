from typing import Optional, TYPE_CHECKING

from test_tools.private.scope.scope_singleton import context

if TYPE_CHECKING:
    from test_tools.private.logger.logger_wrapper import LoggerWrapper


class LoggerInterfaceBase:
    __BELOW_CALLER_STACK_FRAME: int = 1

    def __init__(self, instance: Optional['LoggerWrapper'] = None, *, message_prefix: str = ''):
        self.__message_prefix = message_prefix
        self.__instance: Optional['LoggerWrapper'] = instance

    @property
    def _logger(self) -> 'LoggerWrapper':
        return self.__instance if self.__instance is not None else context.get_logger()

    def debug(self, message: str):
        self._logger.debug(f'{self.__message_prefix}{message}', stacklevel=self.__BELOW_CALLER_STACK_FRAME)

    def info(self, message: str):
        self._logger.info(f'{self.__message_prefix}{message}', stacklevel=self.__BELOW_CALLER_STACK_FRAME)
