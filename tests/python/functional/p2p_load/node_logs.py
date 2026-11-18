"""The lines the harness reads from a node's logs: Block stats, p2p block arrival offsets and sampled relay timestamps."""

from __future__ import annotations

import json
import re
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path

_BLOCK_STATS = re.compile(r"Block stats:(\{.*\})\s*$")
_P2P_OFFSET = re.compile(r"Got (\d+) transactions on block (\d+) by (\S+) -- Block Time Offset: (-?\d+) ms")
# one sampled transaction: BRD when a node broadcasts it (origin and every relay), RCV when one receives it from a peer
_RELAY = re.compile(r"RLY (RCV|BRD) id=(\w+) t=(\d+)")


def log_files(node_dir: Path) -> list[Path]:
    """
    Return the node's log files, each once.

    test-tools keeps one stderr_N.log per run of the node and points stderr.log at the latest; reading both would
    count that run's lines twice, so symlinks are skipped.
    """
    files = sorted(node_dir.glob("stderr_*.log"))
    if not files and (node_dir / "stderr.log").is_file():
        files = [node_dir / "stderr.log"]
    files += sorted(p for p in (node_dir / "logs").rglob("*.log") if p.is_file() and not p.is_symlink())
    return [f for f in files if not f.is_symlink()]


def _lines(node_dir: Path) -> Iterator[str]:
    for path in log_files(node_dir):
        with path.open(errors="replace") as file:
            yield from file


def block_stats(node_dir: Path) -> dict[int, dict[str, Any]]:
    """Return the last `Block stats` report per block number."""
    stats: dict[int, dict[str, Any]] = {}
    for line in _lines(node_dir):
        match = _BLOCK_STATS.search(line)
        if match is None:
            continue
        try:
            report = json.loads(match.group(1))
        except json.JSONDecodeError:  # a line cut by the node's exit
            continue
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
    """Return {sampled transaction id: {"RCV"/"BRD": first timestamp in us}} for one node."""
    samples: dict[str, dict[str, int]] = {}
    for line in _lines(node_dir):
        for kind, tx_id, stamp in _RELAY.findall(line):
            samples.setdefault(tx_id, {}).setdefault(kind, int(stamp))
    return samples
