import inspect
import logging
from pathlib import Path
from typing import Optional


class LoggerWrapper:
    def __init__(self, name, *, parent: Optional['LoggerWrapper'], level=logging.DEBUG, propagate=True):
        self.parent = parent

        self.internal_logger: logging.Logger
        if self.parent is None:
            self.internal_logger = logging.getLogger('root')
        else:
            self.internal_logger = self.parent.internal_logger.getChild(name)
            self.internal_logger.propagate = propagate

        self.internal_logger.setLevel(level)
        self.__file_handler = None

    def __repr__(self):
        return f'<LoggerWrapper: {self.internal_logger.name}>'

    def create_child_logger(self, name: str, child_type: 'LoggerWrapper' = None):
        if child_type is None:
            child_type = LoggerWrapper

        return child_type(name, parent=self)

    def set_file_handler(self, path):
        formatter = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s (%(file_name)s:%(line_number)s)')

        self.__file_handler = logging.FileHandler(path, mode='w')
        self.__file_handler.setFormatter(formatter)
        self.__file_handler.setLevel(logging.DEBUG)
        self.internal_logger.addHandler(self.__file_handler)

    def log_to_file(self):
        # Break import-cycle
        from test_tools.private.scope import context  # pylint: disable=import-outside-toplevel
        self.set_file_handler(context.get_current_directory().joinpath('last_run.log'))

    def debug(self, message, stacklevel=0):
        self.internal_logger.debug(message, extra=self.capture_call_place(stacklevel + 1))

    def info(self, message, stacklevel=0):
        self.internal_logger.info(message, extra=self.capture_call_place(stacklevel + 1))

    @staticmethod
    def capture_call_place(stack_frames_above):
        # When Python 3.8+ will be available use stacklevel keyword argument instead below hacking
        # Example: logger.info('example', stacklevel=1)  # I don't know if 1 is OK

        stack = inspect.stack()
        caller_frame = stack[stack_frames_above + 1]

        return {
            'file_name': Path(caller_frame.filename).name,
            'line_number': caller_frame.lineno
        }
