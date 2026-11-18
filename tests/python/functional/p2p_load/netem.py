"""tc netem delay on the loopback traffic of chosen p2p ports (each colony's endpoint), each way."""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path
    from types import TracebackType
    from typing import Self

_PRIOMAP = ["1", "2", "2", "2", "1", "2", "0", "0", "1", "1", "1", "1", "1", "1", "1", "1"]


class NetemDelay:
    """
    Delays every packet to or from the given ports on `lo` by `delay_ms`; a link's RTT grows by twice that.

    Needs passwordless sudo for tc. Refuses (tc fails) when `lo` already has a root qdisc, so it never
    replaces someone else's shaping; the run lock keeps two harness runs from colliding here.
    """

    def __init__(self, ports: list[int], delay_ms: int, stats_path: Path) -> None:
        self._ports = ports
        self._delay_ms = delay_ms
        self._stats_path = stats_path
        self._installed = False

    @staticmethod
    def _tc(*args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(["sudo", "-n", "tc", *args], capture_output=True, text=True, check=True, timeout=60)

    def __enter__(self) -> Self:
        # band 4 (handle 1:4) only gets what the filters below send there; everything else keeps the prio defaults
        self._tc("qdisc", "add", "dev", "lo", "root", "handle", "1:", "prio", "bands", "4", "priomap", *_PRIOMAP)
        self._installed = True
        try:
            self._tc(*f"qdisc add dev lo parent 1:4 handle 40: netem delay {self._delay_ms}ms limit 1000000".split())
            for port in self._ports:
                for direction in ("dport", "sport"):
                    self._tc(
                        *f"filter add dev lo parent 1: protocol ip prio 1 u32 match ip {direction} {port} 0xffff flowid 1:4".split()
                    )
        except BaseException:
            self._remove()
            raise
        return self

    def __exit__(
        self, exc_type: type[BaseException] | None, exc: BaseException | None, traceback: TracebackType | None
    ) -> None:
        try:
            self._stats_path.write_text(self._tc("-s", "qdisc", "show", "dev", "lo").stdout)
        finally:
            self._remove()

    def _remove(self) -> None:
        if self._installed:
            self._tc("qdisc", "del", "dev", "lo", "root")
            self._installed = False
