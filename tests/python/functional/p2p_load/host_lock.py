"""One harness series per host: a lock in /dev/shm shared by every checkout, plus the old harnesses' containers."""

from __future__ import annotations

import contextlib
import fcntl
import json
import os
import re
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Callable
    from types import TracebackType
    from typing import Self

LOCK_PATH = Path("/dev/shm/hive-p2p-load.lock")
CONTAINER_PREFIX = "p2p-load-"
# this harness's run containers and those of the host-directory harnesses it replaces (w877, w877x, w877y, w877z)
_ACTIVE_CONTAINER = re.compile(rf"^({re.escape(CONTAINER_PREFIX)}|w877)")


class HarnessBusyError(Exception):
    """Another harness run is active on this host."""


def running_container_names() -> list[str]:
    result = subprocess.run(
        ["docker", "ps", "--format", "{{.Names}}"], capture_output=True, text=True, check=True, timeout=60
    )
    return result.stdout.split()


class HostLock:
    """
    Held for a whole series; refuses to start while another run holds it or a harness container runs.

    The lock is an flock, so it is released when its holder dies, however it dies.
    """

    def __init__(
        self,
        path: Path = LOCK_PATH,
        list_containers: Callable[[], list[str]] = running_container_names,
    ) -> None:
        self._path = path
        self._list_containers = list_containers
        self._fd: int | None = None

    def __enter__(self) -> Self:
        fd = os.open(self._path, os.O_RDWR | os.O_CREAT, 0o666)
        # the umask narrows the creation mode; widen it so other accounts on the host can take the lock too
        # (only the file's owner may, and the owner did it when creating the file)
        with contextlib.suppress(PermissionError):
            os.fchmod(fd, 0o666)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            holder = os.pread(fd, 4096, 0).decode(errors="replace").strip()
            os.close(fd)
            raise HarnessBusyError(f"another harness run holds {self._path}: {holder or 'unknown holder'}") from None
        self._fd = fd
        try:
            self._refuse_if_containers_run()
        except BaseException:
            self.__exit__(None, None, None)
            raise
        me = {"pid": os.getpid(), "host": socket.gethostname(), "since": time.time(), "argv": sys.argv}
        os.ftruncate(fd, 0)
        os.pwrite(fd, json.dumps(me).encode(), 0)
        return self

    def _refuse_if_containers_run(self) -> None:
        active = [name for name in self._list_containers() if _ACTIVE_CONTAINER.match(name)]
        if active:
            raise HarnessBusyError(f"harness containers are running on this host: {', '.join(active)}")

    def __exit__(
        self, exc_type: type[BaseException] | None, exc: BaseException | None, traceback: TracebackType | None
    ) -> None:
        if self._fd is not None:
            with contextlib.suppress(OSError):
                os.ftruncate(self._fd, 0)
            os.close(self._fd)
            self._fd = None
