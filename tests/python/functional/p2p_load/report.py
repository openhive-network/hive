"""Text and JSON reports of finished runs, and the per-label comparison of an A/B series."""

from __future__ import annotations

import statistics
from collections import defaultdict
from typing import TYPE_CHECKING, Any

from python.functional.p2p_load import analysis

if TYPE_CHECKING:
    from collections.abc import Callable


def summarize(
    run: analysis.Run,
    full_txs: int | None = None,
    blocks_per_window: int = 10,
    busy_cores: float = analysis.BUSY_CORES,
) -> dict[str, Any]:
    """
    Every metric of one run.

    `full_txs` is the transaction count below which a block counts as underfilled (default 90% of the target);
    `busy_cores` the cores busy outside the run above which the host counts as busy.
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
        }
        | {"colony_mixes": meta.get("colony_mixes", {}), "colony_late_blocks": meta.get("colony_late_blocks", {})},
        "p2p_parameters": {
            "given": meta["p2p_parameters"]["given"],
            "unsupported": meta["p2p_parameters"]["unsupported"],
        },
        "blocks": [meta["first"], meta["last"]],
        "window": analysis.window_shortfall(run),
        "host_load": analysis.host_load(run, busy_cores),
        "remote_host_load": analysis.host_load(run, busy_cores, "remote") if run.remote_roles else None,
        "rates": analysis.rates(run, blocks_per_window),
        "underfilled": {"below_txs": threshold, "blocks": analysis.underfilled(run, threshold)},
        "offsets": analysis.corrected_offsets(run),
        "skew_idle_blocks": analysis.skew_idle_blocks(run),
        "thread_cpu": analysis.thread_cpu(run),
        "sends": analysis.sends(run),
        "relay": analysis.relay_latency(run),
        "relay_coverage": analysis.relay_coverage(run),
        "p2p_loggers": meta.get("p2p_loggers", {}),
        "p2p_counters": analysis.p2p_counters(run),
        "fast_confirms": analysis.fast_confirms(run),
        "irreversibility_lag": analysis.irreversibility_lag(run),
        "late_observer": meta.get("late_observer"),
        "timings": meta.get("timings"),
        "replay": meta.get("replay"),
        "placement": _placement(run),
    }


def _placement(run: analysis.Run) -> dict[str, Any] | None:
    """A two-host run's hosts with their nodes and the measured offset of their clocks; None for a single-host run."""
    meta = run.meta
    if not run.remote_roles:
        return None
    clock = meta["clock"]
    return {
        "local": {"host": meta.get("host"), "roles": [role for role in run.roles if role not in run.remote_roles]},
        "remote": {"host": meta["remote_host"], "roles": run.remote_roles},
        "endpoints": meta.get("endpoints", {}),
        "clock": {
            key: clock.get(key)
            for key in ("offset_s", "uncertainty_s", "min_rtt_s", "median_rtt_s", "probes", "ntp_offset_s")
        },
    }


def _fmt(value: float | None, digits: int = 0) -> str:
    return "-" if value is None else f"{value:.{digits}f}"


def _format_counters(role: str, counters: dict[str, Any] | None) -> list[str]:
    if counters is None:
        return [f"   p2p stats {role:10} - (no report period wholly inside the loaded window)"]
    fetch, handoff = counters["fetch"], counters["handoff"]
    send, requests = counters["send"], counters["transaction_requests"]
    latency = " ".join(f"{bucket} {_fmt(pct)}%" for bucket, pct in handoff["latency_pct"].items())
    indent = " " * 25
    return [
        (
            f"   p2p stats {role:10} {counters['reports']} reports, {counters['seconds']:.0f} s: "
            f"fetch {_fmt(fetch['examined_per_requested'], 2)} examined/requested, "
            f"{fetch['items_requested_s']:.0f} requested/s, {fetch['wakes_s']:.0f} wakes/s, "
            f"scheduling {fetch['scheduling_pct']:.1f}%, {fetch['items_expired']:.0f} expired"
        ),
        (
            f"{indent}handoff {handoff['submissions_s']:.0f} submitted/s, {handoff['completions_s']:.0f} completed/s, "
            f"in flight max {handoff['max_in_flight']:.0f} mean {_fmt(handoff['mean_in_flight'], 1)}, "
            f"{handoff['limit_waits']:.0f} limit waits ({handoff['limit_wait_ms']:.0f} ms); latency {latency}"
        ),
        (
            f"{indent}send {send['writes_s']:.0f} writes/s, {_fmt(send['messages_per_write'], 2)} messages/write, "
            f"{_fmt(send['bytes_per_write'])} B/write; transaction requests {requests['requests_s']:.0f}/s, "
            f"{_fmt(requests['transactions_per_request'], 1)} tx/request, byte cap hit "
            f"{requests['byte_cap_bound']:.0f} ({_fmt(requests['byte_cap_bound_pct'], 1)}%)"
        ),
    ]


def _format_all_counters(summary: dict[str, Any]) -> list[str]:
    if not summary["p2p_counters"]:
        with_loggers = sorted(role for role, loggers in summary["p2p_loggers"].items() if "p2p_stats" in loggers)
        return [
            f"   p2p stats: no reports in the logs (p2p_stats enabled on {', '.join(with_loggers)})"
            if with_loggers
            else "   p2p stats: none (p2p_stats enabled on no node: off, or no binary has it)"
        ]
    return [line for role, counters in summary["p2p_counters"].items() for line in _format_counters(role, counters)]


def _format_fast_confirms(summary: dict[str, Any]) -> list[str]:
    lines = [
        (
            f"   fast-confirms {role:10} {fc['accepted']}/{fc['produced']} accepted ({fc['accepted_pct']:.1f}%), "
            f"ms after the witness: median {_fmt(fc['median_ms'], 1)} max {_fmt(fc['max_ms'], 1)}"
        )
        for role, fc in summary["fast_confirms"].items()
    ] or ["   fast-confirms: none logged by the witness (default logger below debug)"]
    lines.extend(
        f"   head-LIB {role:10} median {lag['median']:g} max {lag['max']} over {lag['blocks']} blocks"
        for role, lag in summary["irreversibility_lag"].items()
    )
    return lines


def _format_host_load(load: dict[str, Any] | None, host: str = "host") -> list[str]:
    if load is None:
        return [f"   {host}: no host CPU in the samples (recorded before the harness sampled it)"]
    lines = [
        (
            f"   {host}: {load['other_busy_cores']:.1f} cores busy outside the run "
            f"(max {load['max_other_busy_cores']:.1f} over a sample interval), "
            f"host {load['host_busy_cores']:.1f} of {_fmt(load['nproc'])} cores busy, run {load['run_cores']:.1f}"
        )
    ]
    if load["busy"]:
        lines.append(
            f"   BUSY {host.upper()}: {load['other_busy_cores']:.1f} cores busy outside the run, "
            f"over {load['busy_cores']:g}; "
            "block offsets and CPU shares are inflated, leave the run out of A/B conclusions"
        )
    return lines


def _format_host_loads(summary: dict[str, Any]) -> list[str]:
    lines = _format_host_load(summary["host_load"])
    if summary["placement"] is not None:
        lines.extend(_format_host_load(summary["remote_host_load"], "remote host"))
    return lines


def _format_timings(summary: dict[str, Any]) -> list[str]:
    timings, replay = summary["timings"], summary["replay"]
    if timings is None or replay is None:
        return []  # recorded before the driver timed its phases
    if replay["templates"] is None:
        state = f"replayed on every node in parallel {replay['replay_s']:.1f} s"
    else:
        reused = sum(1 for t in replay["templates"].values() if t["reused"])
        replayed = (
            f", replayed on {', '.join(replay['replayed'])} {replay['replay_s']:.1f} s" if replay["replayed"] else ""
        )
        state = (
            f"replay templates reused {reused}/{len(replay['templates'])}{replayed}, "
            f"copied into nodes {replay['copy_s']:.1f} s"
        )
    return [
        (
            f"   driver {timings['total_s']:.0f} s: state {timings['state_s']:.1f} s ({state}), "
            f"start to live {timings['start_to_live_s']:.1f} s, live to end {timings['live_to_end_s']:.0f} s, "
            f"close {timings['close_s']:.1f} s"
        )
    ]


def _format_placement(placement: dict[str, Any] | None) -> list[str]:
    if placement is None:
        return []
    clock = placement["clock"]
    ntp = clock["ntp_offset_s"]
    return [
        (
            f"   two hosts: {placement['local']['host']} ({','.join(placement['local']['roles'])}) and "
            f"{placement['remote']['host']} ({','.join(placement['remote']['roles'])}); remote clock "
            f"{clock['offset_s'] * 1000:+.3f} ms +-{clock['uncertainty_s'] * 1000:.3f} ms (NTP "
            f"{'-' if ntp is None else f'{ntp * 1000:+.3f} ms'}), probe RTT min {clock['min_rtt_s'] * 1000:.3f} "
            f"median {clock['median_rtt_s'] * 1000:.3f} ms; relay samples corrected by the offset"
        )
    ]


def _format_skew_idle_blocks(skew: dict[str, Any]) -> list[str]:
    counts = skew["per_node"]
    if not counts:
        return []
    lines = [f"   skew idle blocks: fewest {skew['fewest']}, most {max(counts.values())} per node"]
    if skew["few"]:
        lines.append(
            "   FEW IDLE BLOCKS for the skew: "
            + ", ".join(f"{role} {counts[role]}" for role in skew["few"])
            + f" (wants {analysis.STABLE_IDLE_BLOCKS}; under {analysis.MIN_IDLE_BLOCKS} no offset is corrected)"
        )
    return lines


def format_summary(summary: dict[str, Any], min_thread_pct: float = 2.0) -> str:
    rates = summary["rates"]
    setup = summary["setup"]
    lines = [
        (
            f"== {summary['run']}  label={summary['label'] or '-'} host={summary['host']} "
            f"{setup['topology']} tpb={setup['tpb']} colonies={setup['colonies']} relays={setup['relays']} "
            f"netem={setup['netem_ms']}ms late={setup['late_observer_blocks']} mix={setup['mix']}"
            + "".join(f" colony{i}.mix={mix}" for i, mix in sorted(setup["colony_mixes"].items()))
            + "".join(f" colony{i}.late={k}" for i, k in sorted(setup["colony_late_blocks"].items()))
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
    window = summary["window"]
    if window["short"]:
        lines.append(
            f"   SHORT WINDOW: {window['loaded_blocks']} loaded blocks of {window['expected_blocks']} expected"
            + (
                ", the colonies started late (the witness was at or past colony_start_block when the nodes were live)"
                if window["late_start"]
                else ""
            )
        )
    lines.extend(_format_host_loads(summary))
    lines.extend(_format_placement(summary["placement"]))
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
    lines.extend(_format_skew_idle_blocks(summary["skew_idle_blocks"]))
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
    coverage = summary["relay_coverage"]
    if summary["relay"]:
        lines.append(
            f"   relay samples: {coverage['sampled']} ids over {coverage['nodes']} nodes, "
            f"{coverage['on_every_node']} on every one of them"
        )
    else:
        lines.append("   relay: no sampled transaction timestamps in the logs")
    lines.extend(_format_all_counters(summary))
    lines.extend(_format_fast_confirms(summary))
    if summary["late_observer"]:
        lines.append(f"   late observer: {summary['late_observer']}")
    lines.extend(_format_timings(summary))
    return "\n".join(lines)


def _comparison_rows(summaries: list[dict[str, Any]]) -> dict[str, Callable[[dict[str, Any]], float | None]]:
    def offset_of(role: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: (s["offsets"].get(role) or {}).get("median_ms")

    def relay_of(role: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: (s["relay"].get(role) or {}).get("median_ms")

    def cpu_of(thread: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: s["thread_cpu"].get("witness", {}).get(thread)

    def p2p_cpu_us_per_received_tx(s: dict[str, Any]) -> float | None:
        cpu_pct, received = s["thread_cpu"].get("witness", {}).get("p2p"), s["rates"]["received_tx_s"]
        return cpu_pct * 1e4 / received if cpu_pct is not None and received else None

    def counter_of(role: str, group: str, key: str) -> Callable[[dict[str, Any]], float | None]:
        return lambda s: ((s["p2p_counters"].get(role) or {}).get(group) or {}).get(key)

    rows: dict[str, Callable[[dict[str, Any]], float | None]] = {
        "loaded blocks": lambda s: s["rates"]["blocks"],
        "short windows": lambda s: int(s["window"]["short"]),
        "busy host runs": lambda s: int(_busy(s)),
        "cores busy outside the run": lambda s: (s["host_load"] or {}).get("other_busy_cores"),
        "remote cores busy outside the run": lambda s: (s["remote_host_load"] or {}).get("other_busy_cores"),
        "included tx/s": lambda s: s["rates"]["included_tx_s"],
        "received tx/s": lambda s: s["rates"]["received_tx_s"],
        "underfilled blocks": lambda s: len(s["underfilled"]["blocks"]),
        "fewest skew idle blocks": lambda s: s["skew_idle_blocks"]["fewest"],
        "witness p2p cpu%": cpu_of("p2p"),
        "witness total cpu%": cpu_of("TOTAL"),
        "witness p2p cpu us/received tx": p2p_cpu_us_per_received_tx,
    }
    roles = sorted({role for s in summaries for role in s["offsets"]})
    rows.update({f"{role} corrected offset median ms": offset_of(role) for role in roles})
    relay_roles = sorted({role for s in summaries for role in s["relay"]})
    rows.update({f"{role} relay median ms": relay_of(role) for role in relay_roles})
    # the relaying nodes' counters; the colonies' and the observer's are in each run's report
    counter_roles = sorted({
        role for s in summaries for role in s["p2p_counters"] if role == "witness" or role.startswith("relay")
    })
    for role in counter_roles:
        rows.update({
            f"{role} fetch examined/requested": counter_of(role, "fetch", "examined_per_requested"),
            f"{role} handoff completed/s": counter_of(role, "handoff", "completions_s"),
            f"{role} handoff in flight max": counter_of(role, "handoff", "max_in_flight"),
            f"{role} send messages/write": counter_of(role, "send", "messages_per_write"),
            f"{role} send bytes/write": counter_of(role, "send", "bytes_per_write"),
            f"{role} tx/request": counter_of(role, "transaction_requests", "transactions_per_request"),
            f"{role} byte cap hit %": counter_of(role, "transaction_requests", "byte_cap_bound_pct"),
        })
    rows["fewest fast-confirms accepted %"] = lambda s: min(
        (fc["accepted_pct"] for fc in s["fast_confirms"].values()), default=None
    )
    rows["slowest fast-confirm median ms"] = lambda s: max(
        (fc["median_ms"] for fc in s["fast_confirms"].values() if fc["median_ms"] is not None), default=None
    )
    rows["largest head-LIB"] = lambda s: max((lag["max"] for lag in s["irreversibility_lag"].values()), default=None)
    rows["driver wall s"] = lambda s: (s["timings"] or {}).get("total_s")
    return rows


def _busy(summary: dict[str, Any]) -> bool:
    """On a busy host: either host of a two-host run."""
    return any(load and load["busy"] for load in (summary["host_load"], summary["remote_host_load"]))


def format_comparison(summaries: list[dict[str, Any]]) -> str:
    """
    Mean of each headline metric per label, with the per-run values, in run order.

    When any run is on a busy host, each cell also gives the mean without the busy runs (`quiet`), and the busy runs'
    values are marked with `*`.
    """
    by_label: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for summary in summaries:
        by_label[summary["label"] or "-"].append(summary)
    any_busy = any(map(_busy, summaries))
    width = 40 if any_busy else 28
    lines = ["metric".ljust(40) + "".join(label.rjust(width) for label in by_label)]
    if any_busy:
        lines.append("(* a run on a busy host; quiet: the mean without those runs)")
    for metric, value_of in _comparison_rows(summaries).items():
        cells = []
        for runs in by_label.values():
            measured = [(v, _busy(s)) for s, v in zip(runs, map(value_of, runs), strict=True) if v is not None]
            mean = statistics.mean(v for v, _ in measured) if measured else None
            if any_busy:
                quiet = [v for v, busy in measured if not busy]
                values = "/".join(_fmt(v) + ("*" if busy else "") for v, busy in measured)
                cell = f"{_fmt(mean, 1)} quiet {_fmt(statistics.mean(quiet) if quiet else None, 1)} ({values})"
            else:
                cell = f"{_fmt(mean, 1)} ({'/'.join(_fmt(v) for v, _ in measured)})"
            cells.append(cell.rjust(width))
        lines.append(metric.ljust(40) + "".join(cells))
    return "\n".join(lines)
