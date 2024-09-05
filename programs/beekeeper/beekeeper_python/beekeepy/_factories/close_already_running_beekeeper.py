from __future__ import annotations

import errno
import os
import signal
import time
from typing import TYPE_CHECKING

from loguru import logger

from beekeepy._factories.helpers import _get_logger
from beekeepy._handle.beekeeper import Beekeeper as SynchronousBeekeeperHandle
from beekeepy._interface.settings import Settings
from beekeepy.exceptions import BeekeeperAlreadyRunningError

if TYPE_CHECKING:
    from pathlib import Path


def _is_running(pid: int) -> bool:
    """
    Check whether pid exists in the current process table.

    Source: https://stackoverflow.com/a/7654102

    Args:
    ----
    pid: The Process ID to check.

    Returns:
    -------
    True if process with the given pid is running else False.
    """
    try:
        os.kill(pid, 0)
    except OSError as err:
        if err.errno == errno.ESRCH:
            return False
    return True


def _wait_for_pid_to_die(pid: int, *, timeout_secs: float = 5.0) -> None:
    sleep_time = min(1.0, timeout_secs)
    already_waited = 0.0
    while not _is_running(pid):
        if timeout_secs - already_waited <= 0:
            raise TimeoutError(f"Process with pid {pid} didn't die in {timeout_secs} seconds.")

        time.sleep(sleep_time)
        already_waited += sleep_time


def close_already_running_beekeeper(*, working_directory: Path) -> None:
    """If beekeeper has been started and explicitly not closed, this function allows to close it basing on workdir."""
    try:
        with SynchronousBeekeeperHandle(settings=Settings(working_directory=working_directory), logger=_get_logger()):
            """
            While trying to start new beekeeper instance in same directory,
            there will be send notification with informations about already existing process.
            """
    except BeekeeperAlreadyRunningError as err:
        sig = signal.SIGINT  # signal type which will be used to kill beekeeper
        pid = err.pid  # PID extracted from error, which comes from notification
        os.kill(pid, sig)  # try to kill it

        try:
            _wait_for_pid_to_die(pid, timeout_secs=10)  # check is process actually dead
            logger.debug("Process was closed with SIGINT")
        except TimeoutError:
            sig = signal.SIGKILL  # in case of no reaction to ^C, kill process hard way
            os.kill(pid, sig)
            _wait_for_pid_to_die(pid)  # confirm is hard way take effect
            logger.debug("Process was force-closed with SIGKILL")
    else:
        # If notification did not arrived, report it to user
        logger.warning("BeekeeperAlreadyRunningError did not raise, is other beekeeper running?")
