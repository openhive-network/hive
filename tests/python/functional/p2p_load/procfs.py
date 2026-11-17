"""What /proc tells about running hived processes and the host: which one serves a data dir, CPU ticks, p2p links."""

from __future__ import annotations

import itertools
import os
from pathlib import Path

_PROC = Path("/proc")


def hived_pid(data_dir: Path) -> int | None:
    """Return the pid of the hived running with `--data-dir data_dir`, or None when there is none."""
    wanted = str(data_dir)
    for entry in _PROC.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if (entry / "comm").read_text().strip() != "hived":
                continue
            argv = (entry / "cmdline").read_bytes().decode(errors="replace").split("\0")
        except OSError:  # the process exited while we looked
            continue
        if any(pair == ("--data-dir", wanted) for pair in itertools.pairwise(argv)) or f"--data-dir={wanted}" in argv:
            return int(entry.name)
    return None


def thread_ticks(pid: int) -> dict[str, list[str | int]]:
    """Return {tid: [thread name, user+system clock ticks]} for every thread of the process."""
    out: dict[str, list[str | int]] = {}
    for task in (_PROC / str(pid) / "task").iterdir():
        try:
            stat = (task / "stat").read_text()
        except OSError:  # the thread exited while we looked
            continue
        name = stat[stat.index("(") + 1 : stat.rindex(")")]
        fields = stat[stat.rindex(")") + 2 :].split()
        out[task.name] = [name, int(fields[11]) + int(fields[12])]
    return out


def host_busy_ticks() -> int:
    """Return the clock ticks every CPU of the host spent busy (all but idle and iowait) since boot, from /proc/stat."""
    fields = [int(value) for value in (_PROC / "stat").read_text().splitlines()[0].split()[1:]]
    # user nice system idle iowait irq softirq steal; guest time is counted in user and nice already
    user, nice, system, _, _, irq, softirq, steal = fields[:8]
    return user + nice + system + irq + softirq + steal


def cpu_count() -> int:
    """Return how many CPUs /proc/stat counts, the CPUs host_busy_ticks covers."""
    lines = (_PROC / "stat").read_text().splitlines()
    return sum(1 for line in lines if line.startswith("cpu") and line[3].isdigit())


def tree_ticks(root: int) -> int:
    """
    Return the user+system clock ticks of the process `root` and every descendant of it, waited-for children included.

    A child that exits and is waited for moves its ticks into its parent's, so the sum does not drop when it ends.
    """
    parent: dict[int, int] = {}
    ticks: dict[int, int] = {}
    for entry in _PROC.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text()
        except OSError:  # the process exited while we looked
            continue
        fields = stat[stat.rindex(")") + 2 :].split()
        parent[int(entry.name)] = int(fields[1])
        ticks[int(entry.name)] = sum(int(value) for value in fields[11:15])  # utime stime cutime cstime
    children: dict[int, list[int]] = {}
    for pid, ppid in parent.items():
        children.setdefault(ppid, []).append(pid)
    total, pending = 0, [root]
    while pending:
        pid = pending.pop()
        total += ticks.get(pid, 0)
        pending.extend(children.get(pid, []))
    return total


def _socket_owners(pids: dict[str, int]) -> dict[str, str]:
    owners: dict[str, str] = {}
    for role, pid in pids.items():
        for fd in (_PROC / str(pid) / "fd").iterdir():
            try:
                target = os.readlink(fd)
            except OSError:  # the descriptor was closed while we looked
                continue
            if target.startswith("socket:["):
                owners[target[8:-1]] = role
    return owners


def tcp_links(pids: dict[str, int]) -> dict[str, list[str]]:
    """Return, per role, the roles it has an established TCP connection with (from /proc/net/tcp)."""
    inode_owner = _socket_owners(pids)
    listening: dict[int, str] = {}
    established: list[tuple[str, int, int]] = []
    for line in (_PROC / "net" / "tcp").read_text().splitlines()[1:]:
        fields = line.split()
        owner = inode_owner.get(fields[9])
        local_port, remote_port = int(fields[1].split(":")[1], 16), int(fields[2].split(":")[1], 16)
        if owner is not None and fields[3] == "0A":
            listening[local_port] = owner
        elif owner is not None and fields[3] == "01":
            established.append((owner, local_port, remote_port))
    # an outgoing connection's remote port is the peer's listening port, an accepted one's is the peer's local port
    by_local_port = {local: owner for owner, local, _ in established}
    links: dict[str, set[str]] = {role: set() for role in pids}
    for owner, _, remote_port in established:
        other = listening.get(remote_port) or by_local_port.get(remote_port)
        if other is not None and other != owner:
            links[owner].add(other)
    return {role: sorted(peers) for role, peers in links.items()}
