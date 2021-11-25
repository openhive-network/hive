import logging
import sys
from typing import Optional


class LoggerWrapper:
    __FORMATTER = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s (%(filename)s:%(lineno)s)')

    def __init__(self, name, *, parent: Optional['LoggerWrapper'], level=logging.DEBUG, propagate=True):
        self.parent = parent

        self.internal_logger: logging.Logger
        if self.parent is None:
            self.internal_logger = logging.getLogger('root')
        else:
            self.internal_logger = self.parent.internal_logger.getChild(name)
            self.internal_logger.propagate = propagate

        self.internal_logger.setLevel(level)
        self.__file_handler: Optional[logging.FileHandler] = None
        self.__stream_handler: Optional[logging.StreamHandler] = None

    def __repr__(self):
        return f'<LoggerWrapper: {self.internal_logger.name}>'

    def create_child_logger(self, name: str, child_type: 'LoggerWrapper' = None):
        if child_type is None:
            child_type = LoggerWrapper

        return child_type(name, parent=self)

    def set_file_handler(self, path):
        self.__file_handler = logging.FileHandler(path, mode='w')
        self.__file_handler.setFormatter(self.__FORMATTER)
        self.__file_handler.setLevel(logging.DEBUG)
        self.internal_logger.addHandler(self.__file_handler)

    def log_to_file(self):
        # Break import-cycle
        from test_tools.private.scope import context  # pylint: disable=import-outside-toplevel
        self.set_file_handler(context.get_current_directory().joinpath('last_run.log'))

    def log_to_stdout(self):
        self.__stream_handler = logging.StreamHandler(sys.stdout)
        self.__stream_handler.setFormatter(self.__FORMATTER)
        self.__stream_handler.setLevel(logging.INFO)
        logging.root.addHandler(self.__stream_handler)

    def debug(self, message, stacklevel=0):
        self.internal_logger.debug(message, stacklevel=stacklevel + 2)

    def info(self, message, stacklevel=0):
        self.internal_logger.info(message, stacklevel=stacklevel + 2)

    def warning(self, message, stacklevel=0):
        self.internal_logger.warning(message, stacklevel=stacklevel + 2)

    def error(self, message, stacklevel=0):
        self.internal_logger.error(message, stacklevel=stacklevel + 2)

    def critical(self, message, stacklevel=0):
        self.internal_logger.critical(message, stacklevel=stacklevel + 2)
