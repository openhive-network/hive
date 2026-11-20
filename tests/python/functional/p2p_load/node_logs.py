"""
The lines the harness reads from a node's logs: Block stats, p2p block arrival offsets, sampled relay timestamps, the
periodic p2p statistics and the fast-confirms the node accepted.
"""

from __future__ import annotations

import filecmp
import json
import re
from datetime import UTC, datetime
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path

_BLOCK_STATS = re.compile(r"Block stats:(\{.*\})\s*$")
_P2P_OFFSET = re.compile(r"Got (\d+) transactions on block (\d+) by (\S+) -- Block Time Offset: (-?\d+) ms")
# one sampled transaction: BRD when a node broadcasts it (origin and every relay), RCV when one receives it from a peer;
# the measurement patch's format, then the p2p_relay_sample logger's (with the message hash, which the id matches 1:1)
_RELAY = re.compile(r"RLY (RCV|BRD) id=(\w+) t=(\d+)")
_RELAY_SAMPLE = re.compile(r"relay sample (received|broadcast) (\w+) \w+ at (\d+)")
_RELAY_SAMPLE_KINDS = {"received": "RCV", "broadcast": "BRD"}
_P2P_STATISTICS = re.compile(r"p2p statistics: (\{.*\})\s*$")
_FAST_CONFIRM = re.compile(r"Accepted fast-confirm from witness (\S+) for block #(\d+) ")
# what every appender line starts with (after a console color code, if any): the node's (faked) clock, in ms or us
_LOG_TIME = re.compile(r"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)(?:\.(\d+))?")


def log_files(node_dir: Path) -> list[Path]:
    """
    Return the node's log files, each once.

    test-tools keeps one stderr_N.log per run of the node and points stderr.log at the latest; reading both would
    count that run's lines twice, so symlinks are skipped. An appender's segments come in time order.
    """
    files = sorted(node_dir.glob("stderr_*.log"))
    if not files and (node_dir / "stderr.log").is_file():
        files = [node_dir / "stderr.log"]
    for current in sorted(p for p in (node_dir / "logs").rglob("*.log") if p.is_file()):
        files += _appender_segments(current)
    return [f for f in files if not f.is_symlink()]


def _appender_segments(current: Path) -> list[Path]:
    """
    Return the segments of the appender writing `current`: its rotated `<name>.<stamp>` files in stamp order, then
    `current`, unless it is a copy of one of them.

    hived rotates an appender hourly by the node's clock and points `current` at the segment it writes; copying the
    node's directory back turns that link into a regular file identical to that segment. It need not be the newest by
    stamp: the replay runs on the real clock and the run on the faked one, which can be behind it.
    """
    rotated = sorted(p for p in current.parent.glob(f"{current.name}.*") if p.is_file() and not p.is_symlink())
    if current.is_symlink() or any(filecmp.cmp(current, segment, shallow=False) for segment in rotated):
        return rotated
    return [*rotated, current]


def _lines(node_dir: Path) -> Iterator[str]:
    for path in log_files(node_dir):
        with path.open(errors="replace") as file:
            yield from file


def _log_time(line: str) -> float | None:
    """Return the seconds since the epoch, on the node's clock, at which a log line was written (None: no time)."""
    match = _LOG_TIME.search(line)
    if match is None:
        return None
    stamp = datetime.strptime(match.group(1), "%Y-%m-%dT%H:%M:%S").replace(tzinfo=UTC).timestamp()
    fraction = match.group(2) or "0"
    return stamp + float(f"0.{fraction}")


def block_stats(node_dir: Path) -> dict[int, dict[str, Any]]:
    """Return the last `Block stats` report per block number, with the time its line was logged as `log_time`."""
    stats: dict[int, dict[str, Any]] = {}
    for line in _lines(node_dir):
        match = _BLOCK_STATS.search(line)
        if match is None:
            continue
        try:
            report = json.loads(match.group(1))
        except json.JSONDecodeError:  # a line cut by the node's exit
            continue
        if "num" not in report:  # hived reports `{}` for a block flow it has no block for
            continue
        report["log_time"] = _log_time(line)
        stats[report["num"]] = report
    return stats


def p2p_offsets(node_dir: Path) -> dict[int, dict[str, int]]:
    """Return, per block received from a peer, its transaction count and the logged block time offset in ms."""
    offsets: dict[int, dict[str, int]] = {}
    for line in _lines(node_dir):
        match = _P2P_OFFSET.search(line)
        if match is not None:
            offsets[int(match.group(2))] = {"txs": int(match.group(1)), "offset_ms": int(match.group(4))}
    return offsets


def relay_samples(node_dir: Path) -> dict[str, dict[str, int]]:
    """
    Return {sampled transaction id: {"RCV"/"BRD": first timestamp in us}} for one node.

    Reads both the measurement patch's `RLY` lines and the p2p_relay_sample logger's `relay sample` lines.
    """
    samples: dict[str, dict[str, int]] = {}
    for line in _lines(node_dir):
        found = _RELAY.findall(line) + [
            (_RELAY_SAMPLE_KINDS[kind], tx_id, stamp) for kind, tx_id, stamp in _RELAY_SAMPLE.findall(line)
        ]
        for kind, tx_id, stamp in found:
            samples.setdefault(tx_id, {}).setdefault(kind, int(stamp))
    return samples


def p2p_statistics(node_dir: Path) -> list[dict[str, Any]]:
    """
    Return the node's `p2p statistics` reports in log order, each with the time its line was logged as `log_time`.

    A report covers the `seconds` before that time; fc writes large integers and every double as JSON strings, which
    are left as they are.
    """
    reports: list[dict[str, Any]] = []
    for line in _lines(node_dir):
        match = _P2P_STATISTICS.search(line)
        if match is None:
            continue
        try:
            report = json.loads(match.group(1))
        except json.JSONDecodeError:  # a line cut by the node's exit
            continue
        report["log_time"] = _log_time(line)
        reports.append(report)
    return reports


def fast_confirms(node_dir: Path) -> dict[tuple[str, int], float]:
    """
    Return {(witness, approved block number): the time it was logged} of every fast-confirm the node accepted.

    hived logs an accepted fast-confirm only with the default logger at debug; the producing node logs its own too.
    """
    accepted: dict[tuple[str, int], float] = {}
    for line in _lines(node_dir):
        match = _FAST_CONFIRM.search(line)
        if match is not None:
            logged = _log_time(line)
            if logged is not None:
                accepted.setdefault((match.group(1), int(match.group(2))), logged)
    return accepted
