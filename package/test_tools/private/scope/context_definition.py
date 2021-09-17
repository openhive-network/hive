from copy import deepcopy
from pathlib import Path
from typing import Optional, Union, TYPE_CHECKING

from test_tools.private.names import Names

if TYPE_CHECKING:
    from test_tools.private.logger.logger_wrapper import LoggerWrapper


class Context:
    DEFAULT_CURRENT_DIRECTORY = Path('./generated').absolute()

    def __init__(self, *, parent: Optional['Context']):
        self.__current_directory: Path

        self.__parent: Optional['Context'] = parent

        if self.__parent is not None:
            self.__current_directory = self.__parent.get_current_directory()
            self.__logger = self.__parent.get_logger()
            self.__names = deepcopy(self.__parent.get_names())
        else:
            self.__current_directory = self.DEFAULT_CURRENT_DIRECTORY
            self.__logger = None
            self.__names = Names()

    def get_current_directory(self) -> Path:
        return self.__current_directory

    def set_current_directory(self, directory: Union[str, Path]):
        self.__current_directory = Path(directory)

    def get_logger(self) -> 'LoggerWrapper':
        return self.__logger

    def set_logger(self, logger: 'LoggerWrapper'):
        self.__logger = logger

    def get_names(self) -> Names:
        return self.__names
