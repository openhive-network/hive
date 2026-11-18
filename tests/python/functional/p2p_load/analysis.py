"""
Metrics of one finished run, from its out.json and the node logs copied back to its generated/ directory.

The loaded window starts RAMP_BLOCKS after the first witness block carrying transactions and ends at the run's
last block; idle blocks are the witness's empty blocks before that first loaded one.
"""

from __future__ import annotations

import itertools
import json
import re
import statistics
from collections import defaultdict
from dataclasses import dataclass, field
from functools import cached_property
from typing import TYPE_CHECKING, Any

from python.functional.p2p_load import node_logs

if TYPE_CHECKING:
    from collections.abc import Callable
    from pathlib import Path

BLOCK_INTERVAL_S = 3
RAMP_BLOCKS = 2
MIN_IDLE_BLOCKS = 5
# the fewest idle blocks shown to give the skew of 30 within a millisecond (hive#1098: 28 runs, 111 nodes, at most
# 0.78 ms off); a run with fewer is flagged
STABLE_IDLE_BLOCKS = 8
MIN_RELAY_SAMPLES = 20
# cores busy outside the run above which a run is on a busy host: on steem-2/4/5/13 (2026-11-18) 84 s windows of quiet
# stretches stayed at 0.1-3.3 cores, while other workloads' episodes ran at 6-12
BUSY_CORES = 4.0
# a p2p statistics report gives its period in whole seconds, so its start is known only to about a second
_REPORT_PERIOD_SLACK_S = 1.0
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
    def relay_samples(self) -> dict[str, dict[str, dict[str, int]]]:
        """{sampled transaction id: {role: {"RCV"/"BRD": timestamp in us}}} over every node."""
        seen: dict[str, dict[str, dict[str, int]]] = defaultdict(dict)
        for role in self.roles:
            for tx_id, stamps in node_logs.relay_samples(self.node_dir(role)).items():
                seen[tx_id][role] = stamps
        return dict(seen)

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


def window_shortfall(run: Run) -> dict[str, Any]:
    """
    Whether the loaded window is shorter than the run asked for, and why.

    A run measures `blocks` blocks from colony_start_block, the first RAMP_BLOCKS of them as ramp. `late_start`: the
    witness was at colony_start_block or past it when every node was live, so the colonies started late. `short`: the
    window has fewer than `blocks - RAMP_BLOCKS` witness blocks, or the colonies started late. Runs recorded before
    these fields existed have no expectation and are never short.
    """
    witness = run.stats("witness")
    loaded = sum(1 for num in run.window if num in witness)
    asked = run.meta.get("blocks")
    expected = asked - RAMP_BLOCKS if asked is not None else None
    start = run.meta.get("colony_start_block")
    late_start = start is not None and run.meta["first"] >= start
    return {
        "loaded_blocks": loaded,
        "expected_blocks": expected,
        "late_start": late_start,
        "short": late_start or (expected is not None and loaded < expected),
    }


def host_load(run: Run, busy_cores: float = BUSY_CORES) -> dict[str, Any] | None:
    """
    How busy the host was outside the run over the loaded window, in cores, and whether that makes it a busy host.

    Other-busy cores are the host's busy ticks (/proc/stat) minus those of the driver's process tree (every node, the
    colonies and the driver itself), per second of the window: their mean over the window and their maximum over one
    sample interval. `busy`: the mean exceeds `busy_cores`. None for runs recorded before the samples carried host
    ticks, which are never busy, and for runs with fewer than two samples in the window.
    """
    samples = [s for s in run.meta["samples"] if s["head"] >= run.window.start and "host_busy" in s]
    if len(samples) < 2:
        return None

    def cores(first: dict[str, Any], last: dict[str, Any], key: str) -> float:
        ticks: float = last[key] - first[key]
        seconds: float = last["t"] - first["t"]
        return ticks / float(run.meta["tck"]) / seconds

    first, last = samples[0], samples[-1]
    other = cores(first, last, "host_busy") - cores(first, last, "own")
    return {
        "other_busy_cores": other,
        "max_other_busy_cores": max(
            cores(a, b, "host_busy") - cores(a, b, "own") for a, b in itertools.pairwise(samples)
        ),
        "host_busy_cores": cores(first, last, "host_busy"),
        "run_cores": cores(first, last, "own"),
        "nproc": run.meta.get("nproc"),
        "seconds": last["t"] - first["t"],
        "busy_cores": busy_cores,
        "busy": other > busy_cores,
    }


def underfilled(run: Run, full_txs: int) -> list[tuple[int, int]]:
    """Return (block, transactions) of every loaded-window witness block with fewer than `full_txs` transactions."""
    witness = run.stats("witness")
    return [(num, witness[num]["txs"]) for num in run.window if num in witness and witness[num]["txs"] < full_txs]


def _idle_skews(run: Run, role: str) -> list[float]:
    """Per idle block the node received over p2p: its arrival offset minus the witness's ready offset, in us."""
    witness = run.stats("witness")
    received = {num: s for num, s in run.stats(role).items() if s.get("type") == "p2p"}
    return [received[n]["exec"]["offset"] - witness[n]["offset"] for n in run.idle_blocks if n in received]


def corrected_offsets(run: Run) -> dict[str, dict[str, float] | None]:
    """
    Block offsets of every non-witness node with the clock skew between it and the witness taken out.

    Skew (per node and run) is the median, over idle blocks, of the node's arrival offset minus the witness's
    ready offset: without load a block crosses the link in well under a millisecond, so what is left is the
    difference of the two nodes' faked clocks. A node's corrected offset is its ready offset minus that skew.
    None for a node with fewer than MIN_IDLE_BLOCKS idle blocks (a late joiner, for one).
    """
    out: dict[str, dict[str, float] | None] = {}
    for role in run.roles:
        if role == "witness":
            continue
        received = {num: s for num, s in run.stats(role).items() if s.get("type") == "p2p"}
        idle = _idle_skews(run, role)
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


def skew_idle_blocks(run: Run) -> dict[str, Any]:
    """
    How many idle blocks each non-witness node's skew estimate used, the fewest, and the nodes with fewer than
    STABLE_IDLE_BLOCKS. A late observer joins after the idle blocks by design and counts in neither of the last two.
    """
    counts = {role: len(_idle_skews(run, role)) for role in run.roles if role != "witness"}
    late = run.meta.get("late_observer_blocks", 0) > 0
    counted = {role: n for role, n in counts.items() if not (late and role == "observer")}
    return {
        "per_node": counts,
        "fewest": min(counted.values(), default=None),
        "few": sorted(role for role, n in counted.items() if n < STABLE_IDLE_BLOCKS),
    }


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
    latency: dict[str, list[float]] = defaultdict(list)
    for nodes in run.relay_samples.values():
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


def relay_coverage(run: Run) -> dict[str, int]:
    """
    How many sampled transactions the nodes logged: on any node and on every node that logged samples.

    The sampled transactions are chosen by message hash, so every node should log the same ones.
    """
    per_node: dict[str, set[str]] = defaultdict(set)
    for tx_id, nodes in run.relay_samples.items():
        for role in nodes:
            per_node[role].add(tx_id)
    return {
        "nodes": len(per_node),
        "sampled": len(run.relay_samples),
        "on_every_node": len(set.intersection(*per_node.values())) if per_node else 0,
    }


def _loaded_period(run: Run, role: str) -> tuple[float, float] | None:
    """The loaded window on the node's own clock: from its log of the block before the window to that of its last."""
    logged = {num: s["log_time"] for num, s in run.stats(role).items() if s.get("log_time") is not None}
    nums = [num for num in range(run.window.start - 1, run.window.stop) if num in logged]
    return (logged[nums[0]], logged[nums[-1]]) if len(nums) >= 2 else None


def _counter_totals(reports: list[dict[str, Any]]) -> dict[str, Any]:
    secs = sum(float(r["seconds"]) for r in reports)

    def total(group: str, key: str) -> float:
        return sum(float(r[group][key]) for r in reports)

    def per(numerator: float, denominator: float) -> float | None:
        return numerator / denominator if denominator else None

    def weighted(group: str, key: str, weight: Callable[[dict[str, Any]], float]) -> float | None:
        return per(sum(float(r[group][key]) * weight(r) for r in reports), sum(weight(r) for r in reports))

    handoff_latency = {
        bucket: sum(float(r["handoff"]["completion_latency"][bucket]) for r in reports)
        for bucket in reports[0]["handoff"]["completion_latency"]
    }
    completions = total("handoff", "completions")
    requests = total("transaction_requests", "requests")
    return {
        "reports": len(reports),
        "seconds": secs,
        "fetch": {
            "wakes_s": total("fetch", "wakes") / secs,
            "items_requested_s": total("fetch", "items_requested") / secs,
            "examined_per_requested": per(total("fetch", "candidates_examined"), total("fetch", "items_requested")),
            "items_expired": total("fetch", "items_expired"),
            "scheduling_pct": total("fetch", "scheduling_us") / (secs * 1e6) * 100,
        },
        "handoff": {
            "submissions_s": total("handoff", "submissions") / secs,
            "completions_s": completions / secs,
            "max_in_flight": max(float(r["handoff"]["max_in_flight"]) for r in reports),
            "mean_in_flight": weighted("handoff", "mean_in_flight", lambda r: float(r["handoff"]["submissions"])),
            "latency_pct": {bucket: per(n * 100, completions) for bucket, n in handoff_latency.items()},
            "limit_waits": total("handoff", "limit_waits"),
            "limit_wait_ms": total("handoff", "limit_wait_us") / 1000,
        },
        "send": {
            "writes_s": total("send", "writes") / secs,
            "messages_per_write": weighted("send", "messages_per_write", lambda r: float(r["send"]["writes"])),
            "bytes_per_write": weighted("send", "bytes_per_write", lambda r: float(r["send"]["writes"])),
        },
        "transaction_requests": {
            "requests_s": requests / secs,
            "transactions_per_request": weighted(
                "transaction_requests",
                "transactions_per_request",
                lambda r: float(r["transaction_requests"]["requests"]),
            ),
            "byte_cap_bound": total("transaction_requests", "byte_cap_bound"),
            "byte_cap_bound_pct": per(total("transaction_requests", "byte_cap_bound") * 100, requests),
        },
    }


def p2p_counters(run: Run) -> dict[str, dict[str, Any] | None]:
    """
    Per node, its p2p statistics over the loaded window: the reports whose whole period lies in it, taken together.

    Rates are per second of those periods; ratios (examined per requested, messages and bytes per write,
    transactions per request) are over all of them. Nodes that logged no report are left out; None for a node with
    reports but none wholly inside the loaded window.
    """
    out: dict[str, dict[str, Any] | None] = {}
    for role in run.roles:
        reports = node_logs.p2p_statistics(run.node_dir(role))
        if not reports:
            continue
        period = _loaded_period(run, role)
        inside = [
            r
            for r in reports
            if period is not None
            and r["log_time"] is not None
            and r["log_time"] - float(r["seconds"]) >= period[0] - _REPORT_PERIOD_SLACK_S
            and r["log_time"] <= period[1] + _REPORT_PERIOD_SLACK_S
        ]
        out[role] = _counter_totals(inside) if inside else None
    return out
