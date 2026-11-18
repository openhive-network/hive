"""Text and JSON reports of finished runs, and the per-label comparison of an A/B series."""

from __future__ import annotations

import statistics
from collections import defaultdict
from typing import TYPE_CHECKING, Any

from python.functional.p2p_load import analysis

if TYPE_CHECKING:
    from collections.abc import Callable


def summarize(run: analysis.Run, full_txs: int | None = None, blocks_per_window: int = 10) -> dict[str, Any]:
    """
    Every metric of one run.

    `full_txs` is the transaction count below which a block counts as underfilled (default 90% of the target).
    """
    meta = run.meta
    threshold = full_txs if full_txs is not None else meta["tpb"] * 9 // 10
    return {
        "run": run.name,
        "label": meta.get("label", ""),
        "host": meta.get("host"),
        "setup": {
            key: meta.get(key)
            for key in ("topology", "tpb", "colonies", "relays", "netem_ms", "late_observer_blocks", "mix", "binaries")
        },
        "p2p_parameters": {
            "given": meta["p2p_parameters"]["given"],
            "unsupported": meta["p2p_parameters"]["unsupported"],
        },
        "blocks": [meta["first"], meta["last"]],
        "rates": analysis.rates(run, blocks_per_window),
        "underfilled": {"below_txs": threshold, "blocks": analysis.underfilled(run, threshold)},
        "offsets": analysis.corrected_offsets(run),
        "thread_cpu": analysis.thread_cpu(run),
        "sends": analysis.sends(run),
        "relay": analysis.relay_latency(run),
        "late_observer": meta.get("late_observer"),
    }


def _fmt(value: float | None, digits: int = 0) -> str:
    return "-" if value is None else f"{value:.{digits}f}"


def format_summary(summary: dict[str, Any], min_thread_pct: float = 2.0) -> str:
    rates = summary["rates"]
    setup = summary["setup"]
    lines = [
        (
            f"== {summary['run']}  label={summary['label'] or '-'} host={summary['host']} "
            f"{setup['topology']} tpb={setup['tpb']} colonies={setup['colonies']} relays={setup['relays']} "
            f"netem={setup['netem_ms']}ms late={setup['late_observer_blocks']} mix={setup['mix']}"
        ),
        f"   p2p parameters given {summary['p2p_parameters']['given'] or '(none: binary defaults)'}"
        + (
            f", left out (unknown to the binary) {summary['p2p_parameters']['unsupported']}"
            if summary["p2p_parameters"]["unsupported"]
            else ""
        ),
        (
            f"   blocks {rates['blocks']} loaded: included {rates['included_tx_s']:.0f} tx/s, "
            f"received {rates['received_tx_s']:.0f} tx/s, median {rates['txs_per_block_median']:.0f} tx/block"
        ),
        "   per window: "
        + "  ".join(
            f"{w['from']}-{w['to']} {w['included_tx_s']:.0f}/{w['received_tx_s']:.0f}" for w in rates["windows"]
        ),
    ]
    under = summary["underfilled"]
    lines.append(f"   underfilled (<{under['below_txs']} tx): {len(under['blocks'])} {under['blocks'][:20]}")
    for role, off in summary["offsets"].items():
        if off is None:
            lines.append(f"   offset {role:10} - (fewer than {analysis.MIN_IDLE_BLOCKS} idle blocks to correct skew)")
        else:
            lines.append(
                f"   offset {role:10} median {off['median_ms']:7.1f} ms  p95 {off['p95_ms']:7.1f} ms  "
                f"(skew {off['skew_ms']:.1f} ms over {off['idle_blocks']} idle blocks, {off['blocks']} blocks)"
            )
    for role, threads in summary["thread_cpu"].items():
        shown = "  ".join(f"{name}={pct:.1f}" for name, pct in threads.items() if pct >= min_thread_pct)
        lines.append(f"   cpu% {role:10} {shown}")
    if summary["sends"] is not None:
        for key, value in summary["sends"].items():
            lines.append(f"   witness {key:20} {value['calls_s']:9.0f} calls/s  {value['bytes_per_call']:7.0f} B/call")
    for role, lat in summary["relay"].items():
        lines.append(
            f"   relay {role:10} n={lat['n']:5d} ms after origin: median {lat['median_ms']:7.1f} "
            f"p90 {lat['p90_ms']:7.1f} p99 {lat['p99_ms']:7.1f}"
        )
    if not summary["relay"]:
        lines.append("   relay: no sampled transaction timestamps in the logs")
    if summary["late_observer"]:
        lines.append(f"   late observer: {summary['late_observer']}")
    return "\n".join(lines)


def _comparison_rows(summaries: list[dict[str, Any]]) -> dict[str, Callable[[dict[str, Any]], float | None]]:
    def offset_of(role: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: (s["offsets"].get(role) or {}).get("median_ms")

    def relay_of(role: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: (s["relay"].get(role) or {}).get("median_ms")

    def cpu_of(thread: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: s["thread_cpu"].get("witness", {}).get(thread)

    rows: dict[str, Callable[[dict[str, Any]], float | None]] = {
        "included tx/s": lambda s: s["rates"]["included_tx_s"],
        "received tx/s": lambda s: s["rates"]["received_tx_s"],
        "underfilled blocks": lambda s: len(s["underfilled"]["blocks"]),
        "witness p2p cpu%": cpu_of("p2p"),
        "witness total cpu%": cpu_of("TOTAL"),
    }
    roles = sorted({role for s in summaries for role in s["offsets"]})
    rows.update({f"{role} corrected offset median ms": offset_of(role) for role in roles})
    relay_roles = sorted({role for s in summaries for role in s["relay"]})
    rows.update({f"{role} relay median ms": relay_of(role) for role in relay_roles})
    return rows


def format_comparison(summaries: list[dict[str, Any]]) -> str:
    """Mean of each headline metric per label, with the per-run values, in run order."""
    by_label: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for summary in summaries:
        by_label[summary["label"] or "-"].append(summary)
    lines = ["metric".ljust(40) + "".join(label.rjust(28) for label in by_label)]
    for metric, value_of in _comparison_rows(summaries).items():
        cells = []
        for runs in by_label.values():
            values = [v for v in map(value_of, runs) if v is not None]
            mean = statistics.mean(values) if values else None
            cells.append(f"{_fmt(mean, 1)} ({'/'.join(_fmt(v) for v in values)})".rjust(28))
        lines.append(metric.ljust(40) + "".join(cells))
    return "\n".join(lines)
