"""
The second host of a two-host run, over ssh: copying what its nodes need, its host lock, the offset of its clock
from this host's, and its part of a run.

Standard library only, like harness.py. The remote host runs the same p2p-load, copied to the same path, with its
own python3; every path a run uses (harness, binaries, tools, block log, output root) is the same on both hosts.
"""

from __future__ import annotations

import contextlib
import json
import os
import re
import shlex
import socket
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any, cast

from python.functional.p2p_load.host_lock import HarnessBusyError

if TYPE_CHECKING:
    from collections.abc import Generator, Iterable
    from typing import IO

_SSH_OPTIONS = ("BatchMode=yes", "ConnectTimeout=20", "ServerAliveInterval=15", "ServerAliveCountMax=4")
_SSH = ("ssh", *(word for option in _SSH_OPTIONS for word in ("-o", option)))
_PACKAGE = Path(__file__).resolve().parent
# what the remote p2p-load needs of this checkout: the package and the packages it is imported through
HARNESS_PATHS = (_PACKAGE.parents[1] / "__init__.py", _PACKAGE.parent / "__init__.py", _PACKAGE)
_P2P_LOAD = _PACKAGE / "p2p-load"
_CLOCK_PROBES = 50
_CLOCK_PROBE = "import sys, time\nfor _ in sys.stdin: print(repr(time.time()), flush=True)"
_CHRONY_SYSTEM_TIME = re.compile(r"^System time\s*:\s*([\d.]+) seconds (slow|fast) of NTP time", re.MULTILINE)
_CHRONY_REFERENCE = re.compile(r"^Reference ID\s*:\s*(.+?)\s*$", re.MULTILINE)
_TIMESYNC_OFFSET = re.compile(r"^\s*Offset:\s*([+-]?[\d.]+)(us|ms|s)\s*$", re.MULTILINE)
_TIMESYNC_SERVER = re.compile(r"^\s*Server:\s*(.+?)\s*$", re.MULTILINE)
_TIMESYNC_ROOT_DISTANCE = re.compile(r"^\s*Root distance:\s*([\d.]+)(us|ms|s)\b", re.MULTILINE)
_UNIT_S = {"us": 1e-6, "ms": 1e-3, "s": 1.0}
# both NTP clients' views; a host has one or the other (or neither)
_NTP_STATUS = "chronyc tracking 2>&1; timedatectl timesync-status 2>&1; true"
_LOCK_TIMEOUT_S = 120
_PART_EXIT_TIMEOUT_S = 300


class RemoteHostError(Exception):
    """The remote host of a two-host run cannot be used: ssh, a copy or a command there failed."""


def clock_offset(probes: list[tuple[float, float, float]]) -> dict[str, float]:
    """
    The remote clock minus the local one, from probes of (local send time, remote clock, local receive time).

    The remote read its clock somewhere inside the probe's round trip, so taking it at the round trip's midpoint is
    off by at most half the round trip; the probe with the shortest round trip bounds the offset best.
    """
    if not probes:
        raise ValueError("no clock probes")
    sent, remote, received = min(probes, key=lambda probe: probe[2] - probe[0])
    return {
        "offset_s": remote - (sent + received) / 2,
        "uncertainty_s": (received - sent) / 2,
        "min_rtt_s": received - sent,
        "median_rtt_s": statistics.median(probe[2] - probe[0] for probe in probes),
        "probes": len(probes),
    }


def ntp_status(text: str) -> dict[str, Any]:
    """
    How far the system clock is ahead of NTP time (s; negative: behind), and by whose account.

    From `chronyc tracking`, else from `timedatectl timesync-status` (systemd-timesyncd, whose offset is the
    server's clock minus this one's; its root distance bounds how well it knows NTP time). `source` None: neither.
    """
    system_time = _CHRONY_SYSTEM_TIME.search(text)
    if system_time is not None:
        reference = _CHRONY_REFERENCE.search(text)
        return {
            "source": "chrony",
            "reference": reference.group(1) if reference else None,
            "ahead_of_ntp_s": float(system_time.group(1)) * (1 if system_time.group(2) == "fast" else -1),
        }
    offset = _TIMESYNC_OFFSET.search(text)
    if offset is not None:
        server = _TIMESYNC_SERVER.search(text)
        distance = _TIMESYNC_ROOT_DISTANCE.search(text)
        return {
            "source": "timesyncd",
            "reference": server.group(1) if server else None,
            "ahead_of_ntp_s": -float(offset.group(1)) * _UNIT_S[offset.group(2)],
            "root_distance_s": float(distance.group(1)) * _UNIT_S[distance.group(2)] if distance else None,
        }
    return {"source": None, "reference": None, "ahead_of_ntp_s": None}


def local_address_towards(remote_address: str) -> str:
    """The address this host sends from to `remote_address` (no packet is sent)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect((remote_address, 9))
        return str(probe.getsockname()[0])


def merge_remote_part(out: dict[str, Any], remote_out: dict[str, Any], clock: dict[str, Any]) -> dict[str, Any]:
    """
    out.json of a two-host run: the local part's, with the remote part's nodes added.

    The remote part's samples join the local ones, marked `"part": "remote"`: each holds the ticks of its own host's
    nodes, its host's busy ticks and the head of a node there. `remote` keeps what holds for the remote host alone; `clock` is the offset measured before the run.
    """
    parameters, remote_parameters = out["p2p_parameters"], remote_out["p2p_parameters"]
    return {
        **out,
        "roles": {**out["roles"], **remote_out["roles"]},
        "p2p_parameters": {
            **parameters,
            "applied": {**parameters["applied"], **remote_parameters["applied"]},
            "unsupported": {**parameters["unsupported"], **remote_parameters["unsupported"]},
        },
        "p2p_loggers": {**out["p2p_loggers"], **remote_out["p2p_loggers"]},
        "node_hived_options": {
            **out.get("node_hived_options", {}),
            **remote_out.get("node_hived_options", {}),
        },
        "samples": [*out["samples"], *({**sample, "part": "remote"} for sample in remote_out["samples"])],
        "seed_links_s": max(out["seed_links_s"], remote_out["seed_links_s"]),
        "remote": {
            key: remote_out.get(key)
            for key in ("container_host", "pids", "links", "seed_links_s", "first", "last", "nproc", "tck", "timings")
        },
        "clock": clock,
    }


class RemoteHost:
    def __init__(self, host: str) -> None:
        self.host = host

    def _command(self, argv: Iterable[str]) -> list[str]:
        return [*_SSH, self.host, shlex.join(argv)]

    def run(self, *argv: str, timeout: float = 120) -> str:
        """Run a command on the host; return its stdout. Raise RemoteHostError when it fails."""
        try:
            result = subprocess.run(self._command(argv), capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise RemoteHostError(f"{self.host}: {shlex.join(argv)} took over {timeout} s") from error
        if result.returncode != 0:
            raise RemoteHostError(
                f"{self.host}: {shlex.join(argv)} exited {result.returncode}: {result.stderr.strip()[-1000:]}"
            )
        return result.stdout

    def check_access(self) -> None:
        """
        Raise RemoteHostError unless ssh reaches the host without a password; say when only the ssh agent does.

        Every step of a series opens its own ssh connection, so access that rests on an agent, a forwarded one most
        of all, ends the series when the agent goes.
        """
        agent = os.environ.get("SSH_AUTH_SOCK", "")
        if subprocess.run(self._command(["true"]), capture_output=True, check=False, timeout=120).returncode != 0:
            if not agent:
                how = "no ssh agent here (SSH_AUTH_SOCK is unset) and no key of this host is accepted there"
            elif not Path(agent).is_socket():
                how = f"the ssh agent socket SSH_AUTH_SOCK={agent} is gone (a dropped agent forward?)"
            else:
                how = f"neither the ssh agent at {agent} nor a key of this host is accepted there"
            raise RemoteHostError(f"ssh {self.host} fails without a password: {how}; give this host a key it accepts")
        keys_only = [*_SSH[:1], "-o", "IdentityAgent=none", *_SSH[1:], self.host, "true"]
        if agent and subprocess.run(keys_only, capture_output=True, check=False, timeout=120).returncode != 0:
            print(
                f"warning: {self.host} accepts only the ssh agent at {agent}: the series ends if the agent goes",
                file=sys.stderr,
                flush=True,
            )

    def address(self) -> str:
        """The host's IPv4 address, as this host resolves its name."""
        try:
            return str(socket.getaddrinfo(self.host, None, socket.AF_INET, socket.SOCK_STREAM)[0][4][0])
        except socket.gaierror as error:
            raise RemoteHostError(f"{self.host} has no IPv4 address: {error}") from error

    def free_ports(self, count: int) -> list[int]:
        """`count` TCP ports free on the host now, picked there as harness.free_ports picks them here."""
        return [int(port) for port in self.run(str(_P2P_LOAD), "free-ports", str(count)).split()]

    def copy(self, paths: Iterable[Path]) -> None:
        """Copy files and directories to the same absolute paths on the host; rsync sends only what changed."""
        # no directory times: -R sends the implied parents too (/home...), whose times only their owner may set
        command = ["rsync", "-aR", "--omit-dir-times", "--exclude", "__pycache__", "-e", shlex.join(_SSH)]
        result = subprocess.run(
            [*command, *sorted({str(path) for path in paths}), f"{self.host}:/"], capture_output=True, text=True
        )
        if result.returncode != 0:
            raise RemoteHostError(f"copying to {self.host} failed: {result.stderr.strip()[-1000:]}")

    def fetch(self, remote_dir: Path, local_dir: Path) -> None:
        """Copy a directory of the host into `local_dir`."""
        local_dir.mkdir(parents=True, exist_ok=True)
        command = ["rsync", "-a", "-e", shlex.join(_SSH), f"{self.host}:{remote_dir}/", f"{local_dir}/"]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise RemoteHostError(f"copying {remote_dir} back from {self.host} failed: {result.stderr.strip()}")

    @contextlib.contextmanager
    def hold_lock(self) -> Generator[None]:
        """
        Hold the harness host lock on the host while the context lasts.

        `p2p-load hold-lock` there holds it until its ssh connection closes, so the lock goes when this process
        goes, however it goes. Raise HarnessBusyError when the host is busy.
        """
        holder = subprocess.Popen(
            self._command([str(_P2P_LOAD), "hold-lock"]),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        stdin, stdout = cast("IO[str]", holder.stdin), cast("IO[str]", holder.stdout)
        try:
            if stdout.readline().strip() != "locked":
                _, error = holder.communicate(timeout=_LOCK_TIMEOUT_S)
                failure = HarnessBusyError if holder.returncode == 2 else RemoteHostError
                # the remote p2p-load says "not starting: <why>" (exit 2), as this one will in turn
                why = error.strip().removeprefix("not starting: ")
                raise failure(f"{self.host}: {why or f'hold-lock exited {holder.returncode}'}")
            yield
        finally:
            with contextlib.suppress(OSError):
                stdin.close()
            try:
                holder.wait(timeout=_LOCK_TIMEOUT_S)
            except subprocess.TimeoutExpired:
                holder.kill()
                holder.wait()

    def measure_clock(self) -> dict[str, Any]:
        """
        The host's clock minus this host's, by direct probes over one ssh connection, with both hosts' NTP view.

        `ntp_offset_s` is the same offset as the hosts' NTP clients see it: the difference of the two clocks'
        distances from NTP time, each known only to that client's own accuracy.
        """
        prober = subprocess.Popen(
            self._command(["python3", "-u", "-c", _CLOCK_PROBE]),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )
        stdin, stdout = cast("IO[str]", prober.stdin), cast("IO[str]", prober.stdout)
        probes: list[tuple[float, float, float]] = []
        try:
            for _ in range(_CLOCK_PROBES):
                sent = time.time()
                stdin.write("\n")
                stdin.flush()
                line = stdout.readline()
                received = time.time()
                if not line:
                    raise RemoteHostError(f"{self.host}: the clock probe ended early")
                probes.append((sent, float(line), received))
        finally:
            stdin.close()
            prober.wait(timeout=60)
        clock: dict[str, Any] = {"host": self.host, **clock_offset(probes)}
        local = subprocess.run(["sh", "-c", _NTP_STATUS], capture_output=True, text=True, check=False)
        ntp = {"local": ntp_status(local.stdout), "remote": ntp_status(self.run("sh", "-c", _NTP_STATUS))}
        clock["ntp"] = ntp
        aheads = [ntp[side]["ahead_of_ntp_s"] for side in ("remote", "local")]
        clock["ntp_offset_s"] = None if None in aheads else aheads[0] - aheads[1]
        return clock

    def start_part(self, request: dict[str, Any], log: Path) -> RemotePart:
        return RemotePart(self._command([str(_P2P_LOAD), "part"]), request, log)

    def clean_part(self, name: str) -> list[str]:
        """Remove what run `name`'s part left on the host (its container, its tmpfs directory); return what it was."""
        return self.run(str(_P2P_LOAD), "clean-part", name, timeout=_PART_EXIT_TIMEOUT_S).splitlines()


class RemotePart:
    """
    The remote host's part of one run: its p2p-load `part` over ssh, steered through the connection's stdin.

    The part says "ready" once its nodes are replayed; `go` passes it the shared start, `stop` the local part's end.
    Closing stdin without `stop` (this process ending, the connection breaking) makes the part kill its nodes.
    """

    def __init__(self, command: list[str], request: dict[str, Any], log: Path) -> None:
        self._log = log.open("w")
        self._process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self._log, text=True
        )
        self._stdin, self._stdout = cast("IO[str]", self._process.stdin), cast("IO[str]", self._process.stdout)
        self.ready = threading.Event()
        self._send(json.dumps(request))
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self) -> None:
        for line in self._stdout:
            if line.strip() == "ready":
                self.ready.set()

    def _send(self, line: str) -> None:
        # a part that already ended reads nothing more; its exit code tells why it ended
        with contextlib.suppress(BrokenPipeError):
            self._stdin.write(line + "\n")
            self._stdin.flush()

    def go(self, start: float) -> None:
        """Start the nodes at `start`, on the remote host's clock."""
        self._send(f"go {start!r}")

    def stop(self) -> None:
        self._send("stop")

    def poll(self) -> int | None:
        return self._process.poll()

    def finish(self) -> int:
        """Close the part's stdin and wait for it to end (it kills its nodes when it had no `stop`); its exit code."""
        with contextlib.suppress(OSError):
            self._stdin.close()
        try:
            exit_code = self._process.wait(timeout=_PART_EXIT_TIMEOUT_S)
        except subprocess.TimeoutExpired:
            self._process.kill()
            exit_code = self._process.wait()
        self._log.close()
        return exit_code
