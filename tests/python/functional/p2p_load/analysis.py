"""
Metrics of one finished run, from its out.json and the node logs copied back to its generated/ directory.

The loaded window starts RAMP_BLOCKS after the first witness block carrying transactions and ends at the run's
last block; idle blocks are the witness's empty blocks before that first loaded one.
"""

from __future__ import annotations

import json
import re
import statistics
from collections import defaultdict
from dataclasses import dataclass, field
from functools import cached_property
from typing import TYPE_CHECKING, Any

from python.functional.p2p_load import node_logs

if TYPE_CHECKING:
    from pathlib import Path

BLOCK_INTERVAL_S = 3
RAMP_BLOCKS = 2
MIN_IDLE_BLOCKS = 5
MIN_RELAY_SAMPLES = 20
_BPFTRACE_COUNTER = re.compile(r"^@(calls|bytes)\[(\w+), (.+)\]: (\d+)$")


class RunDataError(Exception):
    """The run directory lacks what a metric needs (no out.json, no loaded blocks...)."""


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * pct / 100
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


@dataclass
class Run:
    directory: Path
    meta: dict[str, Any]
    _stats: dict[str, dict[int, dict[str, Any]]] = field(default_factory=dict, repr=False)

    @classmethod
    def load(cls, directory: Path) -> Run:
        out = directory / "out.json"
        if not out.is_file():
            raise RunDataError(f"{directory}: no out.json (the run did not finish)")
        return cls(directory, json.loads(out.read_text()))

    @property
    def name(self) -> str:
        return str(self.meta.get("name", self.directory.name))

    @property
    def roles(self) -> list[str]:
        return list(self.meta["roles"])

    def node_dir(self, role: str) -> Path:
        return self.directory / "generated" / str(self.meta["roles"][role])

    def stats(self, role: str) -> dict[int, dict[str, Any]]:
        if role not in self._stats:
            self._stats[role] = node_logs.block_stats(self.node_dir(role))
        return self._stats[role]

    @cached_property
    def window(self) -> range:
        """The loaded window's block numbers."""
        produced = {
            num: s for num, s in self.stats("witness").items() if self.meta["first"] <= num <= self.meta["last"]
        }
        loaded = sorted(num for num, s in produced.items() if s["txs"] > 0)
        if not loaded or loaded[0] + RAMP_BLOCKS > self.meta["last"]:
            raise RunDataError(f"{self.name}: the witness produced no loaded blocks after the ramp")
        return range(loaded[0] + RAMP_BLOCKS, self.meta["last"] + 1)

    @cached_property
    def idle_blocks(self) -> list[int]:
        load_start = self.window.start - RAMP_BLOCKS
        return sorted(num for num, s in self.stats("witness").items() if num < load_start and s["txs"] == 0)


def rates(run: Run, blocks_per_window: int = 10) -> dict[str, Any]:
    """
    Included tx/s (in witness blocks) and received tx/s (applied by the witness between blocks), over the loaded window
    and per window of `blocks_per_window` blocks.
    """
    witness = run.stats("witness")
    nums = [num for num in run.window if num in witness]

    def rate_of(chunk: list[int]) -> dict[str, Any]:
        secs = BLOCK_INTERVAL_S * len(chunk)
        return {
            "from": chunk[0],
            "to": chunk[-1],
            "included_tx_s": sum(witness[n]["txs"] for n in chunk) / secs,
            "received_tx_s": sum(witness[n]["before"]["inc"] for n in chunk) / secs,
        }

    total = rate_of(nums)
    total["blocks"] = len(nums)
    total["txs_per_block_median"] = statistics.median(witness[n]["txs"] for n in nums)
    total["windows"] = [rate_of(nums[i : i + blocks_per_window]) for i in range(0, len(nums), blocks_per_window)]
    return total


def underfilled(run: Run, full_txs: int) -> list[tuple[int, int]]:
    """Return (block, transactions) of every loaded-window witness block with fewer than `full_txs` transactions."""
    witness = run.stats("witness")
    return [(num, witness[num]["txs"]) for num in run.window if num in witness and witness[num]["txs"] < full_txs]


def corrected_offsets(run: Run) -> dict[str, dict[str, float] | None]:
    """
    Block offsets of every non-witness node with the clock skew between it and the witness taken out.

    Skew (per node and run) is the median, over idle blocks, of the node's arrival offset minus the witness's
    ready offset: without load a block crosses the link in well under a millisecond, so what is left is the
    difference of the two nodes' faked clocks. A node's corrected offset is its ready offset minus that skew.
    None for a node with fewer than MIN_IDLE_BLOCKS idle blocks (a late joiner, for one).
    """
    witness = run.stats("witness")
    out: dict[str, dict[str, float] | None] = {}
    for role in run.roles:
        if role == "witness":
            continue
        received = {num: s for num, s in run.stats(role).items() if s.get("type") == "p2p"}
        idle = [received[n]["exec"]["offset"] - witness[n]["offset"] for n in run.idle_blocks if n in received]
        loaded = [n for n in run.window if n in received]
        if len(idle) < MIN_IDLE_BLOCKS or not loaded:
            out[role] = None
            continue
        skew = statistics.median(idle)
        corrected = [(received[n]["offset"] - skew) / 1000 for n in loaded]
        out[role] = {
            "skew_ms": skew / 1000,
            "idle_blocks": len(idle),
            "blocks": len(corrected),
            "median_ms": statistics.median(corrected),
            "p95_ms": percentile(corrected, 95),
        }
    return out


def thread_cpu(run: Run) -> dict[str, dict[str, float]]:
    """Per node, CPU of each thread name (all threads of that name together) in % of one core over the loaded window."""
    samples = [s for s in run.meta["samples"] if s["head"] >= run.window.start]
    out: dict[str, dict[str, float]] = {}
    for role in run.roles:
        present = [s for s in samples if role in s["ticks"]]
        if len(present) < 2:
            continue
        first, last = present[0], present[-1]
        secs = last["t"] - first["t"]
        by_name: dict[str, float] = defaultdict(float)
        for tid, (name, ticks) in last["ticks"][role].items():
            by_name[name] += ticks - first["ticks"][role].get(tid, [name, 0])[1]
        pct = {name: ticks / run.meta["tck"] / secs * 100 for name, ticks in by_name.items()}
        pct["TOTAL"] = sum(pct.values())
        out[role] = dict(sorted(pct.items(), key=lambda item: -item[1]))
    return out


def sends(run: Run) -> dict[str, dict[str, float]] | None:
    """
    Witness socket send syscalls per (syscall, thread) from the optional bpftrace capture: calls/s and bytes per call.

    None when the run had no capture.
    """
    capture = run.directory / "sends.txt"
    info = run.directory / "sends.json"
    if not capture.is_file() or not info.is_file():
        return None
    secs = json.loads(info.read_text())["seconds"]
    counters: dict[str, dict[str, int]] = defaultdict(dict)
    for line in capture.read_text(errors="replace").splitlines():
        match = _BPFTRACE_COUNTER.match(line.strip())
        if match:
            kind, syscall, thread, value = match.groups()
            counters[f"{syscall}/{thread}"][kind] = int(value)
    return {
        key: {"calls_s": c.get("calls", 0) / secs, "bytes_per_call": c.get("bytes", 0) / max(c.get("calls", 0), 1)}
        for key, c in sorted(counters.items())
    }


def relay_latency(run: Run) -> dict[str, dict[str, float]]:
    """
    Per node, ms from a sampled transaction's first broadcast (at its origin) to its arrival there.

    Arrival is the node's RCV timestamp, or its own BRD when it logs no RCV. Only transactions seen on at least two
    nodes count, and only nodes with MIN_RELAY_SAMPLES of them are reported. Empty when the build logs no samples.
    """
    seen: dict[str, dict[str, dict[str, int]]] = defaultdict(dict)
    for role in run.roles:
        for tx_id, stamps in node_logs.relay_samples(run.node_dir(role)).items():
            seen[tx_id][role] = stamps
    latency: dict[str, list[float]] = defaultdict(list)
    for nodes in seen.values():
        broadcasts = [(stamps["BRD"], role) for role, stamps in nodes.items() if "BRD" in stamps]
        if len(nodes) < 2 or not broadcasts:
            continue
        origin_time, origin = min(broadcasts)
        for role, stamps in nodes.items():
            if role != origin:
                latency[role].append((stamps.get("RCV", stamps.get("BRD", origin_time)) - origin_time) / 1000)
    return {
        role: {
            "n": len(ms),
            "median_ms": statistics.median(ms),
            "p90_ms": percentile(ms, 90),
            "p99_ms": percentile(ms, 99),
        }
        for role, ms in sorted(latency.items())
        if len(ms) >= MIN_RELAY_SAMPLES
    }
