"""
Host side of a harness run: node data on tmpfs, the driver in the test-tools container, logs and configs copied back.

Uses only the standard library, so it runs with the host's python3; the driver itself runs in the container.
"""

from __future__ import annotations

import contextlib
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
from dataclasses import asdict
from pathlib import Path
from typing import TYPE_CHECKING, Any

from python.functional.p2p_load import node_logs, procfs
from python.functional.p2p_load.host_lock import CONTAINER_PREFIX
from python.functional.p2p_load.netem import NetemDelay
from python.functional.p2p_load.remote import merge_remote_part

if TYPE_CHECKING:
    from collections.abc import Callable, Iterable

    from python.functional.p2p_load.remote import RemoteHost, RemotePart
    from python.functional.p2p_load.spec import RunSpec

TMPFS_ROOT = Path("/dev/shm/hive-p2p-load")
# replayed node state reused by later runs (see replay_template.py); outlives the runs, unlike TMPFS_ROOT/<run>
TEMPLATE_ROOT = Path("/dev/shm/hive-p2p-load-templates")
REPO_ROOT = Path(__file__).resolve().parents[4]
CONTAINER_PYTHON = "/opt/test-tools-venv/bin/python3"
_IMAGE_IN_PROJECT = re.compile(r"registry\.gitlab\.syncad\.com/hive/hive/aidev-tests@sha256:[0-9a-f]{64}")
_RUN_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
# what of a node's directory is copied back: everything but the state, the block log and their artifacts
_NOT_COPIED = shutil.ignore_patterns("blockchain", "*.bin", "block_log*", "*.artifacts")
_SENDS_AFTER_LOADED_BLOCKS = 6
# a two-host run: how far ahead of the start signal both hosts start their nodes, and how often the run is checked
_START_LEAD_S = 5
_POLL_S = 0.5
# a two-host run's p2p ports come from below the ephemeral port range: there, nothing (an outgoing connection, a
# node's webserver on port 0) takes a port between its pick and the node's bind
_P2P_PORT_BASE = 20000
_EPHEMERAL_PORTS = Path("/proc/sys/net/ipv4/ip_local_port_range")
_BPFTRACE_SENDS = """
tracepoint:syscalls:sys_exit_sendto /pid == PID && args->ret > 0/ { @calls["sendto", comm] = count(); @bytes["sendto", comm] = sum(args->ret); }
tracepoint:syscalls:sys_exit_sendmsg /pid == PID && args->ret > 0/ { @calls["sendmsg", comm] = count(); @bytes["sendmsg", comm] = sum(args->ret); }
tracepoint:syscalls:sys_exit_write /pid == PID && args->ret > 0 && (comm == "p2p" || comm == "asio")/ { @calls["write", comm] = count(); @bytes["write", comm] = sum(args->ret); }
tracepoint:syscalls:sys_exit_writev /pid == PID && args->ret > 0 && (comm == "p2p" || comm == "asio")/ { @calls["writev", comm] = count(); @bytes["writev", comm] = sum(args->ret); }
interval:s:SECONDS { exit(); }
"""


class HarnessSetupError(Exception):
    """A run cannot start as requested (missing binary, name in use, no image...)."""


def default_image() -> str:
    """The test-tools image AIDEV runs this checkout's checks in (.aidev/project.yaml), unless P2P_LOAD_IMAGE is set."""
    if os.environ.get("P2P_LOAD_IMAGE"):
        return os.environ["P2P_LOAD_IMAGE"]
    project = REPO_ROOT / ".aidev" / "project.yaml"
    match = _IMAGE_IN_PROJECT.search(project.read_text()) if project.is_file() else None
    if match is None:
        raise HarnessSetupError(f"no aidev-tests image in {project}; pass --image or set P2P_LOAD_IMAGE")
    return match.group(0)


def find_tools(tools_dir: Path | None, hived: Path) -> dict[str, Path]:
    """Locate get_dev_key, block_log_util and cli_wallet in a flat directory or a build tree."""
    roots = [tools_dir] if tools_dir else [hived.parent, *([hived.parents[2]] if len(hived.parents) > 2 else [])]
    layouts = {
        "GET_DEV_KEY_PATH": ["get_dev_key", "programs/util/get_dev_key"],
        "BLOCK_LOG_UTIL_PATH": ["block_log_util", "programs/util/block_log_util"],
        "CLI_WALLET_PATH": ["cli_wallet", "programs/cli_wallet/cli_wallet"],
    }
    found: dict[str, Path] = {}
    for variable, candidates in layouts.items():
        path = next((r / c for r in roots for c in candidates if (r / c).is_file()), None)
        if path is not None:
            found[variable] = path.resolve()
        elif variable != "CLI_WALLET_PATH":
            raise HarnessSetupError(f"no {candidates[0]} under {', '.join(map(str, roots))}; pass --tools")
    return found


def free_port() -> int:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def free_ports(count: int) -> list[int]:
    """`count` TCP ports free on every address of this host now, the first from _P2P_PORT_BASE on."""
    ephemeral_start = int(_EPHEMERAL_PORTS.read_text().split()[0])
    ports: list[int] = []
    for port in range(_P2P_PORT_BASE, ephemeral_start):
        with socket.socket() as probe:
            try:
                probe.bind(("0.0.0.0", port))
            except OSError:  # in use
                continue
        ports.append(port)
        if len(ports) == count:
            return ports
    raise HarnessSetupError(f"fewer than {count} free ports from {_P2P_PORT_BASE} to {ephemeral_start}")


def _write_json(path: Path, data: Any) -> None:
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data))
    tmp.replace(path)


def _append_series_log(out_root: Path, line: str) -> None:
    load = Path("/proc/loadavg").read_text().split()[:3]
    stamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    with (out_root / "series.log").open("a") as log:
        log.write(f"{stamp} {socket.gethostname()} {line} load={'/'.join(load)}\n")


def _docker_command(spec: RunSpec, image: str, tmp_dir: Path, tools: dict[str, Path], part: str) -> list[str]:
    out_dir = Path(spec.out_dir)
    # only this host's binaries: a two-host run copies to each host the binaries of its own nodes
    binaries = {Path(spec.hived), *(Path(spec.binary(role)) for role in spec.roles_on(part))}
    read_only = {REPO_ROOT, *(b.parent for b in binaries), *(t.parent for t in tools.values())}
    writable = {
        out_dir,
        tmp_dir,
        Path(spec.block_log),
        *([Path(spec.replay_templates)] if spec.replay_templates else []),
    }
    env = {
        "HOME": "/tmp",
        "PYTHONPATH": str(REPO_ROOT / "tests"),
        "PYTHONDONTWRITEBYTECODE": "1",
        "TEST_TOOLS_NODE_DEFAULT_WAIT_FOR_LIVE_TIMEOUT": "300",
        "HIVED_PATH": spec.hived,
        **{variable: str(path) for variable, path in tools.items()},
    }
    command = ["docker", "run", "--rm", "--name", CONTAINER_PREFIX + spec.name, "--network", "host"]
    command += ["--user", f"{os.getuid()}:{os.getgid()}", "-w", str(tmp_dir)]
    for path in sorted(read_only - writable):
        command += ["-v", f"{path}:{path}:ro"]
    for path in sorted(writable):
        command += ["-v", f"{path}:{path}"]
    for variable, value in env.items():
        command += ["-e", f"{variable}={value}"]
    driver = [CONTAINER_PYTHON, "-m", "python.functional.p2p_load", "drive", str(out_dir / "spec.json"), "--part", part]
    return [*command, image, *driver]


class _SendsCapture(threading.Thread):
    """Once the witness has produced a few loaded blocks, counts its socket sends with bpftrace for `seconds`."""

    def __init__(self, out_dir: Path, seconds: int, driver: subprocess.Popen[bytes]) -> None:
        super().__init__(daemon=True)
        self._out_dir = out_dir
        self._seconds = seconds
        self._driver = driver
        self.error: str | None = None

    def _witness_dir(self) -> Path | None:
        roles = self._out_dir / "roles.json"
        return Path(json.loads(roles.read_text())["directories"]["witness"]) if roles.is_file() else None

    def run(self) -> None:
        while self._driver.poll() is None:
            witness_dir = self._witness_dir()
            if witness_dir is not None:
                stats = node_logs.block_stats(witness_dir)
                pid = procfs.hived_pid(witness_dir)
                if pid is not None and sum(1 for s in stats.values() if s["txs"] > 0) >= _SENDS_AFTER_LOADED_BLOCKS:
                    self._capture(pid, max(stats))
                    return
            time.sleep(2)
        self.error = "the run ended before the witness produced loaded blocks"

    def _capture(self, pid: int, head: int) -> None:
        script = _BPFTRACE_SENDS.replace("PID", str(pid)).replace("SECONDS", str(self._seconds))
        with (self._out_dir / "sends.txt").open("w") as output:
            result = subprocess.run(["sudo", "-n", "bpftrace", "-e", script], stdout=output, stderr=subprocess.STDOUT)
        info = {"seconds": self._seconds, "pid": pid, "witness_block_at_start": head, "exit": result.returncode}
        (self._out_dir / "sends.json").write_text(json.dumps(info))
        if result.returncode != 0:
            self.error = f"bpftrace exited {result.returncode} (see sends.txt)"


def _create_run_dirs(spec: RunSpec) -> tuple[Path, Path]:
    """
    Create the run's output and tmpfs directories, which must not exist yet, and the template directory it mounts;
    write spec.json.
    """
    if not _RUN_NAME.match(spec.name):
        raise HarnessSetupError(f"run name {spec.name!r} must be letters, digits, '.', '_' or '-'")
    out_dir, tmp_dir = Path(spec.out_dir), TMPFS_ROOT / spec.name
    for directory in (out_dir, tmp_dir):
        if directory.exists():
            raise HarnessSetupError(f"{directory} exists; pick another run name or remove it")
    out_dir.mkdir(parents=True)
    tmp_dir.mkdir(parents=True)
    if spec.replay_templates:
        Path(spec.replay_templates).mkdir(parents=True, exist_ok=True)
    (out_dir / "spec.json").write_text(json.dumps(asdict(spec), indent=1))
    return out_dir, tmp_dir


def _start_driver(
    spec: RunSpec, image: str, tmp_dir: Path, tools: dict[str, Path], part: str
) -> subprocess.Popen[bytes]:
    with (Path(spec.out_dir) / "driver.log").open("wb") as log:
        return subprocess.Popen(_docker_command(spec, image, tmp_dir, tools, part), stdout=log, stderr=log)


def _end_driver(spec: RunSpec, driver: subprocess.Popen[bytes]) -> None:
    """Remove the driver's container if it still runs (the run failed or was aborted), with its nodes."""
    if driver.poll() is None:
        subprocess.run(["docker", "rm", "-f", CONTAINER_PREFIX + spec.name], check=False)
        driver.wait()


def _keep_logs(tmp_dir: Path, out_dir: Path) -> None:
    """Copy the nodes' logs and configs into the run directory and remove the tmpfs directory."""
    if (tmp_dir / "generated").is_dir():
        shutil.copytree(tmp_dir / "generated", out_dir / "generated", symlinks=True, ignore=_NOT_COPIED)
    shutil.rmtree(tmp_dir)


def _start_together(out_dir: Path, driver: subprocess.Popen[bytes], part: RemotePart, offset_s: float) -> int:
    """
    Start both hosts' nodes at one moment once both have replayed, then wait for the local part; the exit code.

    Non-zero when either part failed: one that ends before the start, or a remote part that fails while the local
    one runs, ends the run (the caller removes what still runs).
    """
    while not ((out_dir / "local.ready").is_file() and part.ready.is_set()):
        for name, exit_code in (("local", driver.poll()), ("remote", part.poll())):
            if exit_code is not None:
                print(f"the {name} part ended (exit {exit_code}) before the nodes started", file=sys.stderr)
                return exit_code or 1
        time.sleep(_POLL_S)
    start = time.time() + _START_LEAD_S
    _write_json(out_dir / "local.go", {"t0": start})
    part.go(start + offset_s)
    while driver.poll() is None:
        remote_exit = part.poll()
        if remote_exit:
            print(f"the remote part failed (exit {remote_exit}) while the local part ran", file=sys.stderr)
            return remote_exit
        time.sleep(_POLL_S)
    part.stop()
    remote_exit = part.finish()
    return driver.returncode or remote_exit


def _collect_remote_part(remote: RemoteHost, out_dir: Path) -> None:
    """Copy the remote part's run directory to OUT_DIR/remote and merge its out.json into the run's."""
    remote.fetch(out_dir, out_dir / "remote")
    out, remote_out = out_dir / "out.json", out_dir / "remote" / "out.json"
    if out.is_file() and remote_out.is_file():
        clock = json.loads((out_dir / "clock.json").read_text())
        merged = merge_remote_part(json.loads(out.read_text()), json.loads(remote_out.read_text()), clock)
        _write_json(out, merged)


def execute_run(
    spec: RunSpec,
    out_root: Path,
    image: str,
    tools: dict[str, Path],
    sends_seconds: int = 0,
    remote: RemoteHost | None = None,
) -> int:
    """
    Run one measurement with node data under TMPFS_ROOT; return the driver's exit code.

    The caller holds the host lock (of both hosts, for a two-host run, whose `remote` it passes). Logs, configs,
    spec.json, out.json and driver.log end up in out_root/<name>, the remote host's in out_root/<name>/remote; the
    tmpfs directories are removed whatever happens.
    """
    if bool(spec.remote_host) != (remote is not None):
        raise HarnessSetupError("a two-host run needs its remote host, and only a two-host run takes one")
    out_dir, tmp_dir = _create_run_dirs(spec)
    _append_series_log(out_root, f"start {spec.name} label={spec.label}")
    started = time.time()
    exit_code = -1
    try:
        with contextlib.ExitStack() as stack:
            part, offset_s = None, 0.0
            if remote is not None:
                clock = remote.measure_clock()
                offset_s = clock["offset_s"]
                (out_dir / "clock.json").write_text(json.dumps(clock, indent=1))
                request = {"spec": asdict(spec), "image": image, "tools": {k: str(v) for k, v in tools.items()}}
                part = remote.start_part(request, out_dir / "part.log")
                # leaving early (a failure here, Ctrl-C) closes the part's stdin: it kills its nodes and ends
                stack.callback(part.finish)
            if spec.netem_ms:
                stack.enter_context(NetemDelay(list(spec.colony_ports.values()), spec.netem_ms, out_dir / "tc.txt"))
            driver = _start_driver(spec, image, tmp_dir, tools, "local")
            capture = _SendsCapture(out_dir, sends_seconds, driver) if sends_seconds else None
            if capture:
                capture.start()
            try:
                exit_code = _start_together(out_dir, driver, part, offset_s) if part else driver.wait()
            finally:
                _end_driver(spec, driver)
            if capture:
                capture.join()
                if capture.error:
                    (out_dir / "sends.err").write_text(capture.error + "\n")
    finally:
        try:
            _keep_logs(tmp_dir, out_dir)
            if remote is not None:
                _collect_remote_part(remote, out_dir)
        finally:
            _append_series_log(out_root, f"end {spec.name} exit={exit_code} wall={time.time() - started:.0f}s")
    return exit_code


def _follow_commands(commands: Iterable[str], out_dir: Path, spec: RunSpec, driver: subprocess.Popen[bytes]) -> None:
    stopped = False
    for line in commands:
        word, _, value = line.strip().partition(" ")
        if word == "go":
            _write_json(out_dir / "remote.go", {"t0": float(value)})
        elif word == "stop":
            (out_dir / "remote.stop").touch()
            stopped = True
    if not stopped:  # the run's host gave up on the run, or its connection broke
        _end_driver(spec, driver)


def execute_part(
    spec: RunSpec, image: str, tools: dict[str, Path], commands: Iterable[str], say: Callable[[str], None]
) -> int:
    """
    The remote host's part of a two-host run, steered by the run's host over `commands` (the part's ssh stdin).

    Says "ready" once this host's nodes are replayed. "go START" passes the shared start on this host's clock,
    "stop" the end of the local part; the end of `commands` without "stop" kills the nodes. Keeps the logs and
    configs in the run directory, as execute_run does, and returns the driver's exit code.
    """
    out_dir, tmp_dir = _create_run_dirs(spec)
    out_root = out_dir.parent
    _append_series_log(out_root, f"start {spec.name} part=remote")
    exit_code = -1
    try:
        driver = _start_driver(spec, image, tmp_dir, tools, "remote")
        try:
            threading.Thread(target=_follow_commands, args=(commands, out_dir, spec, driver), daemon=True).start()
            while driver.poll() is None and not (out_dir / "remote.ready").is_file():
                time.sleep(_POLL_S)
            if driver.poll() is None:
                say("ready")
            exit_code = driver.wait()
        finally:
            _end_driver(spec, driver)
    finally:
        _keep_logs(tmp_dir, out_dir)
        _append_series_log(out_root, f"end {spec.name} part=remote exit={exit_code}")
    return exit_code
