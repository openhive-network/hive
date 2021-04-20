import logging
from logging import debug, info, warning, error, critical, exception
from logging import getLogger

from pathlib import Path

__log_path = Path('logs/run.log')


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
    stream_handler = logging.StreamHandler(stdout)
    stream_handler.setFormatter(formatter)
    stream_handler.setLevel(logging.INFO)
    logging.root.addHandler(stream_handler)

    # Configure file handler
    __log_path.parent.mkdir(exist_ok=True)
    file_handler = logging.FileHandler(__log_path)
    file_handler.setFormatter(formatter)
    file_handler.setLevel(logging.DEBUG)
    logging.root.addHandler(file_handler)


__start_logging()
