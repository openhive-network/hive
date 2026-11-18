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
import threading
import time
from dataclasses import asdict
from pathlib import Path
from typing import TYPE_CHECKING

from python.functional.p2p_load import node_logs, procfs
from python.functional.p2p_load.host_lock import CONTAINER_PREFIX
from python.functional.p2p_load.netem import NetemDelay

if TYPE_CHECKING:
    from python.functional.p2p_load.spec import RunSpec

TMPFS_ROOT = Path("/dev/shm/hive-p2p-load")
REPO_ROOT = Path(__file__).resolve().parents[4]
CONTAINER_PYTHON = "/opt/test-tools-venv/bin/python3"
_IMAGE_IN_PROJECT = re.compile(r"registry\.gitlab\.syncad\.com/hive/hive/aidev-tests@sha256:[0-9a-f]{64}")
_RUN_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
# what of a node's directory is copied back: everything but the state, the block log and their artifacts
_NOT_COPIED = shutil.ignore_patterns("blockchain", "*.bin", "block_log*", "*.artifacts")
_SENDS_AFTER_LOADED_BLOCKS = 6
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


def _append_series_log(out_root: Path, line: str) -> None:
    load = Path("/proc/loadavg").read_text().split()[:3]
    stamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    with (out_root / "series.log").open("a") as log:
        log.write(f"{stamp} {socket.gethostname()} {line} load={'/'.join(load)}\n")


def _docker_command(spec: RunSpec, image: str, tmp_dir: Path, tools: dict[str, Path]) -> list[str]:
    out_dir = Path(spec.out_dir)
    binaries = {Path(spec.binary(role)) for role in spec.roles()}
    read_only = {REPO_ROOT, *(b.parent for b in binaries), *(t.parent for t in tools.values())}
    writable = {out_dir, tmp_dir, Path(spec.block_log)}
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
    return [*command, image, CONTAINER_PYTHON, "-m", "python.functional.p2p_load", "drive", str(out_dir / "spec.json")]


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


def execute_run(spec: RunSpec, out_root: Path, image: str, tools: dict[str, Path], sends_seconds: int = 0) -> int:
    """
    Run one measurement with node data under TMPFS_ROOT; return the driver's exit code.

    The caller holds the host lock. Logs, configs, spec.json, out.json and driver.log end up in out_root/<name>;
    the tmpfs directory is removed whatever happens.
    """
    if not _RUN_NAME.match(spec.name):
        raise HarnessSetupError(f"run name {spec.name!r} must be letters, digits, '.', '_' or '-'")
    out_dir, tmp_dir = Path(spec.out_dir), TMPFS_ROOT / spec.name
    for directory in (out_dir, tmp_dir):
        if directory.exists():
            raise HarnessSetupError(f"{directory} exists; pick another run name or remove it")
    out_dir.mkdir(parents=True)
    tmp_dir.mkdir(parents=True)
    (out_dir / "spec.json").write_text(json.dumps(asdict(spec), indent=1))
    _append_series_log(out_root, f"start {spec.name} label={spec.label}")
    exit_code = -1
    try:
        with contextlib.ExitStack() as stack:
            if spec.netem_ms:
                stack.enter_context(NetemDelay(list(spec.colony_ports.values()), spec.netem_ms, out_dir / "tc.txt"))
            with (out_dir / "driver.log").open("wb") as log:
                driver = subprocess.Popen(_docker_command(spec, image, tmp_dir, tools), stdout=log, stderr=log)
            capture = _SendsCapture(out_dir, sends_seconds, driver) if sends_seconds else None
            if capture:
                capture.start()
            try:
                exit_code = driver.wait()
            finally:
                if driver.poll() is None:
                    subprocess.run(["docker", "rm", "-f", CONTAINER_PREFIX + spec.name], check=False)
                    driver.wait()
            if capture:
                capture.join()
                if capture.error:
                    (out_dir / "sends.err").write_text(capture.error + "\n")
    finally:
        if (tmp_dir / "generated").is_dir():
            shutil.copytree(tmp_dir / "generated", out_dir / "generated", symlinks=True, ignore=_NOT_COPIED)
        shutil.rmtree(tmp_dir)
        _append_series_log(out_root, f"end {spec.name} exit={exit_code}")
    return exit_code
