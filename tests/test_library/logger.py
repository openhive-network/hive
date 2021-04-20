import logging
from logging import debug, info, warning, error, critical, exception
from logging import getLogger

from pathlib import Path

__log_path = Path('logs/run.log')
__stream_handler = logging.NullHandler()
__file_handler = logging.NullHandler()


def show_debug_logs_on_stdout():
    __stream_handler.setLevel(logging.DEBUG)


def __remove_old_log_if_exists():
    try:
        __log_path.unlink()
    except FileNotFoundError:
        pass


def __start_logging():
    __remove_old_log_if_exists()

    logging.root.setLevel(logging.DEBUG)

    formatter = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s (%(name)s, %(filename)s:%(lineno)s)')

    # Configure stream handler
    from sys import stdout
    global __stream_handler
    __stream_handler = logging.StreamHandler(stdout)
    __stream_handler.setFormatter(formatter)
    __stream_handler.setLevel(logging.INFO)
    logging.root.addHandler(__stream_handler)

    # Configure file handler
    __log_path.parent.mkdir(exist_ok=True)
    global __file_handler
    __file_handler = logging.FileHandler(__log_path)
    __file_handler.setFormatter(formatter)
    __file_handler.setLevel(logging.DEBUG)
    logging.root.addHandler(__file_handler)

    # Suppress debug logs from selected built-in python libraries
    logging.getLogger('urllib3.connectionpool').setLevel(logging.INFO)


__start_logging()
