import logging
from pathlib import Path


class Logger:
    def __init__(self):
        self.__log_path = Path('logs/run.log')
        self.__stream_handler = logging.NullHandler()
        self.__file_handler = logging.NullHandler()

        self.__start_logging()

    def __start_logging(self):
        self.__remove_old_log_if_exists()

        logging.root.setLevel(logging.DEBUG)

        formatter = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s (%(name)s, %(filename)s:%(lineno)s)')

        # Configure stream handler
        from sys import stdout
        self.__stream_handler = logging.StreamHandler(stdout)
        self.__stream_handler.setFormatter(formatter)
        self.__stream_handler.setLevel(logging.INFO)
        logging.root.addHandler(self.__stream_handler)

        # Configure file handler
        self.__log_path.parent.mkdir(exist_ok=True)
        self.__file_handler = logging.FileHandler(self.__log_path)
        self.__file_handler.setFormatter(formatter)
        self.__file_handler.setLevel(logging.DEBUG)
        logging.root.addHandler(self.__file_handler)

        # Suppress debug logs from selected built-in python libraries
        logging.getLogger('urllib3.connectionpool').setLevel(logging.INFO)

    def __remove_old_log_if_exists(self):
        try:
            self.__log_path.unlink()
        except FileNotFoundError:
            pass

    def show_debug_logs_on_stdout(self):
        __stream_handler.setLevel(logging.DEBUG)

    # Wrapped functions from logging library
    def debug(self, message):
        logging.debug(message)

    def info(self, message):
        logging.info(message)

    def warning(self, message):
        logging.warning(message)

    def error(self, message):
        logging.error(message)

    def critical(self, message):
        logging.critical(message)

    def exception(self, message):
        logging.exception(message)

    def getLogger(self, name):
        return logging.getLogger(name)
