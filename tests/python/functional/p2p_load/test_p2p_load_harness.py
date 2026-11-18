from __future__ import annotations

import contextlib
import json
import os
import shutil
import socket
import subprocess
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

import pytest

import test_tools as tt
from python.functional.p2p_load import __main__ as cli
from python.functional.p2p_load import harness, p2p_loggers, p2p_parameters, procfs, remote, report, topology
from python.functional.p2p_load.analysis import Run
from python.functional.p2p_load.host_lock import HarnessBusyError, HostLock
from python.functional.p2p_load.spec import RunSpec

# `hived --help` as boost prints it: the default JSON is wrapped, without spaces, over indented lines
_HELP_WITH_DELAYS = """  --p2p-parameters arg
          P2P network parameters. (Default: {"listen_endpoint":"0.0.0.0:0","accept_incoming_connections":true,"wait_if_
          endpoint_is_busy":true,"peer_inactivity_timeout":5,"transaction_flush_delay_microseconds":1000,"fetch_items_m
          inimum_interval_microseconds":5000,"maximum_transactions_in_flight":10000} )
  --pacemaker-source arg
"""
_HELP_WITHOUT_DELAYS = """  --p2p-parameters arg
          P2P network parameters. (Default: {"listen_endpoint":"0.0.0.0:0","accept_incoming_connections":true,"wait_if_
          endpoint_is_busy":true,"peer_inactivity_timeout":5} )
"""


def test_only_given_parameters_known_to_the_binary_are_applied() -> None:
    new_build = p2p_parameters.defaults_from_help(_HELP_WITH_DELAYS)
    old_build = p2p_parameters.defaults_from_help(_HELP_WITHOUT_DELAYS)
    assert new_build["fetch_items_minimum_interval_microseconds"] == 5000
    given = {"transaction_flush_delay_microseconds": 0}

    on_new = p2p_parameters.select(given, new_build)
    on_old = p2p_parameters.select(given, old_build)
    unset = p2p_parameters.select({}, new_build)

    assert on_new.applied == {"transaction_flush_delay_microseconds": 0}
    assert on_new.unsupported == []
    assert on_old.applied == {}
    assert on_old.unsupported == ["transaction_flush_delay_microseconds"]
    assert unset.applied == {}


def test_help_without_p2p_parameters_is_refused() -> None:
    with pytest.raises(p2p_parameters.P2pParametersError):
        p2p_parameters.defaults_from_help("  --p2p-endpoint arg\n")


def test_config_check_tells_set_unset_and_wrong_parameters_apart(tmp_path: Path) -> None:
    config = tmp_path / "config.ini"
    config.write_text('p2p-endpoint = 127.0.0.1:2001\np2p-parameters = {"transaction_flush_delay_microseconds": 0}\n')
    p2p_parameters.verify_config(config, {"transaction_flush_delay_microseconds": 0})
    with pytest.raises(p2p_parameters.P2pParametersError):
        p2p_parameters.verify_config(config, {})

    config.write_text("p2p-endpoint = 127.0.0.1:2001\n")
    p2p_parameters.verify_config(config, {})
    with pytest.raises(p2p_parameters.P2pParametersError):
        p2p_parameters.verify_config(config, {"transaction_flush_delay_microseconds": 0})


@pytest.mark.testnet
def test_node_config_carries_exactly_the_applied_parameters() -> None:
    defaults = p2p_parameters.binary_defaults(tt.paths_to_executables.get_path_of("hived"))
    assert "transaction_flush_delay_microseconds" in defaults
    given = {"transaction_flush_delay_microseconds": 0, "no_such_p2p_parameter": 1}
    selection = p2p_parameters.select(given, defaults)
    assert selection.unsupported == ["no_such_p2p_parameter"]

    configured, default = tt.InitNode(), tt.InitNode()
    configured.config.p2p_parameters = json.dumps(selection.applied)
    for node in (configured, default):
        node.run()

    p2p_parameters.verify_config(configured.config_file_path, {"transaction_flush_delay_microseconds": 0})
    p2p_parameters.verify_config(default.config_file_path, {})


def test_a_second_run_on_the_host_is_refused(tmp_path: Path) -> None:
    lock_path = tmp_path / "lock"
    with HostLock(lock_path, list_containers=list), pytest.raises(HarnessBusyError, match="holds"):
        HostLock(lock_path, list_containers=list).__enter__()
    with HostLock(lock_path, list_containers=list):  # released when the first run ended
        pass


@pytest.mark.parametrize("container", ["p2p-load-other-run", "w877-old", "w877x-1075-ch-i-a", "w877z-z77-smoke"])
def test_a_running_harness_container_refuses_the_run(tmp_path: Path, container: str) -> None:
    with pytest.raises(HarnessBusyError, match=container):
        HostLock(tmp_path / "lock", list_containers=lambda: ["unrelated", container]).__enter__()
    with HostLock(tmp_path / "lock", list_containers=lambda: ["unrelated"]):  # the refused attempt left no lock
        pass


_ENDPOINTS = {"witness": "10.0.0.1:2001", "colony0": "10.0.0.2:2002"}


@pytest.mark.parametrize(
    ("changes", "error"),
    [
        ({"topology": "ring"}, "topology"),
        ({"topology": "chain", "colonies": 2}, "one colony"),
        ({"topology": "star", "relays": 1}, "relays"),
        ({"node_hived": {"relay3": "/bin/true"}}, "roles"),
        ({"colony_ports": {"1": 4000}}, "colony index"),
        ({"remote_host": "steem-2"}, "remote roles"),
        ({"remote_roles": ["colony0"]}, "remote host"),
        ({"remote_host": "steem-2", "remote_roles": ["colony7"], "endpoints": _ENDPOINTS}, "colony7"),
        ({"remote_host": "steem-2", "remote_roles": ["witness"], "endpoints": _ENDPOINTS}, "witness runs on the local"),
        ({"remote_host": "steem-2", "remote_roles": ["colony0"], "endpoints": {}}, "no p2p endpoint"),
        (
            {"remote_host": "steem-2", "remote_roles": ["colony0"], "endpoints": _ENDPOINTS, "netem_ms": 50},
            "netem",
        ),
        (
            {
                "remote_host": "steem-2",
                "remote_roles": ["observer"],
                "endpoints": _ENDPOINTS,
                "late_observer_blocks": 2,
            },
            "late observer",
        ),
    ],
)
def test_spec_validation(changes: dict[str, Any], error: str) -> None:
    spec = _spec(**changes)
    with pytest.raises(ValueError, match=error):
        spec.validate()


_T0 = 1_763_445_600  # 2025-11-18T06:00:00 UTC: when block 100 is logged; every block 3 s later


def _log_time(seconds: float, digits: int) -> str:
    stamp = datetime.fromtimestamp(_T0 + seconds, UTC)
    return stamp.strftime("%Y-%m-%dT%H:%M:%S.%f")[: 20 + digits]


def _block_stats(num: int, txs: int, offset_us: int, arrival_us: int, inc: int, kind: str) -> str:
    stats = {
        "num": num,
        "type": kind,
        "txs": txs,
        "offset": offset_us,
        "before": {"inc": inc},
        "exec": {"offset": arrival_us},
    }
    prefix = f"{_log_time(3 * (num - 100), 6)} block_flow_control.cpp:127 on_end_of_apply_block ] "
    return f"{prefix}Block stats:{json.dumps(stats)}\n"


def _p2p_statistics(logged_at: int, requested: int, examined: int, completions: int) -> str:
    """A report as fc writes it: large integers and every double as strings."""
    report = {
        "seconds": 30,
        "fetch": {
            "wakes": 300,
            "candidates_examined": examined,
            "items_requested": requested,
            "items_expired": 0,
            "items_queued": 5,
            "scheduling_us": 300_000,
        },
        "handoff": {
            "submissions": completions,
            "completions": completions,
            "in_flight": 3,
            "max_in_flight": 700 if examined == requested else 9000,
            "mean_in_flight": "120.00000000000000000",
            "completion_latency": {
                "lt100us": 0,
                "lt250us": completions // 4,
                "lt500us": completions // 2,
                "lt1000us": completions // 4,
                "lt2000us": 0,
                "lt5000us": 0,
                "ge5000us": 0,
            },
            "limit_waits": 0,
            "limit_wait_us": 0,
        },
        "send": {
            "writes": 600,
            "messages_per_write": "2.50000000000000000",
            "bytes_per_write": "1000.00000000000000000",
        },
        "transaction_requests": {
            "requests": 200,
            "transactions_per_request": "60.00000000000000000",
            "byte_cap_bound": 10,
        },
    }
    prefix = f"{_log_time(logged_at, 3)} node.cpp:2190                  statistics_report_loop ] "
    return f"{prefix}p2p statistics: {json.dumps(report)}\n"


def _relay_line(relay_format: str, kind: str, i: int, stamp: int) -> str:
    if relay_format == "patch":
        return f"RLY {kind} id={i:016x} t={stamp}\n"
    event = {"BRD": "broadcast", "RCV": "received"}[kind]
    return (
        f"{_log_time(0, 3)} node.cpp:5063 process_trx_message ] relay sample {event} {i:040x} {i + 7:040x} at {stamp}\n"
    )


def _make_run(directory: Path, relay_format: str = "patch") -> Path:
    """
    A finished chain run: blocks 100-109 idle, 110-140 loaded with 1000 tx (block 120 only 500).

    The observer's clock is 20 ms ahead of the witness's and blocks reach it 50 ms after the witness is ready;
    sampled transactions reach relay1 51 ms after colony0 broadcast them, in the measurement patch's `RLY` lines or
    in the p2p_relay_sample logger's lines (which also have them reach the witness 80 ms after). The witness logs
    p2p statistics every 30 s; the reports logged at 90 s and 120 s cover the loaded window (blocks 111-140 are
    logged at 33-120 s), the others carry figures the report must not show.
    """
    roles = {"witness": "WitnessNode0", "observer": "ApiNode0", "colony0": "ApiNode1", "relay1": "ApiNode2"}
    generated = directory / "generated"
    for name in roles.values():
        (generated / name / "logs" / "p2p").mkdir(parents=True)
        (generated / name / "stderr_0.log").write_text("replay\n")
    witness_lines, observer_lines = [], []
    for num in range(100, 141):
        txs = 0 if num < 110 else 500 if num == 120 else 1000
        witness_lines.append(_block_stats(num, txs, 400_000, 0, 1200 if txs else 0, "gen"))
        loaded_delay = 50_000 if txs else 0
        observer_lines.append(
            _block_stats(num, txs, 400_000 + 20_000 + loaded_delay, 400_000 + 20_000 + loaded_delay, 0, "p2p")
        )
    for name, lines in (("WitnessNode0", witness_lines), ("ApiNode0", observer_lines)):
        (generated / name / "stderr_1.log").write_text("".join(lines))
        (generated / name / "stderr.log").symlink_to(generated / name / "stderr_1.log")
    colony_log = "".join(_relay_line(relay_format, "BRD", i, 1_000_000 + i * 1000) for i in range(25))
    relay_log = "".join(_relay_line(relay_format, "RCV", i, 1_051_000 + i * 1000) for i in range(25))
    witness_log = "".join(
        _p2p_statistics(logged_at, 3000, 3000 if 90 <= logged_at <= 120 else 99_000, 12_000)
        for logged_at in (30, 60, 90, 120, 150)
    )
    if relay_format == "sample":
        witness_log += "".join(_relay_line(relay_format, "RCV", i, 1_080_000 + i * 1000) for i in range(25))
    (generated / "ApiNode1" / "logs" / "p2p" / "p2p.log").write_text(colony_log)
    (generated / "ApiNode2" / "logs" / "p2p" / "p2p.log").write_text(relay_log)
    (generated / "WitnessNode0" / "logs" / "p2p" / "p2p.log").write_text(witness_log)

    witness_ticks = [{"1": ["p2p", 0], "2": ["chain", 0]}, {"1": ["p2p", 300], "2": ["chain", 600]}]
    meta = {
        "name": "chain-a-1",
        "label": "a",
        "host": "steem-x",
        "topology": "chain",
        "tpb": 1000,
        "colonies": 1,
        "relays": 1,
        "netem_ms": 0,
        "late_observer_blocks": 0,
        "mix": "default",
        "binaries": {},
        "roles": roles,
        "p2p_parameters": {"given": {}, "unsupported": {}},
        "p2p_loggers": {role: list(p2p_loggers.LOGGERS) for role in roles},
        "first": 100,
        "last": 140,
        "tck": 100,
        "samples": [
            {"t": 1000.0 + 30 * i, "head": 115 + 10 * i, "ticks": {"witness": witness_ticks[i]}} for i in (0, 1)
        ],
        "late_observer": None,
    }
    (directory / "out.json").write_text(json.dumps(meta))
    return directory


@pytest.mark.parametrize("relay_format", ["patch", "sample"])
def test_report_of_a_finished_run(tmp_path: Path, relay_format: str) -> None:
    summary = report.summarize(Run.load(_make_run(tmp_path / "chain-a-1", relay_format)))

    rates = summary["rates"]
    assert rates["blocks"] == 29  # 112-140: the loaded window starts two blocks after the first loaded one
    assert rates["included_tx_s"] == pytest.approx((28 * 1000 + 500) / (3 * 29))
    assert rates["received_tx_s"] == pytest.approx(1200 / 3)
    assert summary["underfilled"]["blocks"] == [(120, 500)]

    observer = summary["offsets"]["observer"]
    assert observer["skew_ms"] == pytest.approx(20)
    assert observer["median_ms"] == pytest.approx(450)
    assert observer["idle_blocks"] == 10
    assert summary["offsets"]["colony0"] is None  # no Block stats from it: nothing to correct

    assert summary["thread_cpu"]["witness"]["p2p"] == pytest.approx(10)
    assert summary["thread_cpu"]["witness"]["TOTAL"] == pytest.approx(30)
    assert summary["relay"]["relay1"]["median_ms"] == pytest.approx(51)
    assert summary["relay"]["relay1"]["n"] == 25
    assert summary["sends"] is None
    assert "relay relay1" in report.format_summary(summary)

    counters = summary["p2p_counters"]
    assert list(counters) == ["witness"]  # the only node logging p2p statistics
    witness = counters["witness"]
    assert witness["reports"] == 2
    assert witness["seconds"] == 60
    assert witness["fetch"]["examined_per_requested"] == pytest.approx(1.0)
    assert witness["fetch"]["scheduling_pct"] == pytest.approx(1.0)
    assert witness["handoff"]["completions_s"] == pytest.approx(rates["received_tx_s"])
    assert witness["handoff"]["max_in_flight"] == 700
    assert witness["handoff"]["mean_in_flight"] == pytest.approx(120)
    assert witness["handoff"]["latency_pct"]["lt500us"] == pytest.approx(50)
    assert witness["send"]["writes_s"] == pytest.approx(20)
    assert witness["send"]["messages_per_write"] == pytest.approx(2.5)
    assert witness["send"]["bytes_per_write"] == pytest.approx(1000)
    assert witness["transaction_requests"]["transactions_per_request"] == pytest.approx(60)
    assert witness["transaction_requests"]["byte_cap_bound_pct"] == pytest.approx(5)
    assert "p2p stats witness" in report.format_summary(summary)


def test_an_empty_block_stats_line_is_skipped(tmp_path: Path) -> None:
    run_dir = _make_run(tmp_path / "chain-a-1")
    for name in ("WitnessNode0", "ApiNode0"):
        log = run_dir / "generated" / name / "stderr_1.log"
        lines = log.read_text().splitlines(keepends=True)
        empty = f"{_log_time(3 * 15, 6)} block_flow_control.cpp:127 on_end_of_apply_block ] Block stats:{{}}\n"
        log.write_text("".join([*lines[:15], empty, *lines[15:]]))

    summary = report.summarize(Run.load(run_dir))

    assert summary["rates"]["blocks"] == 29
    assert summary["rates"]["included_tx_s"] == pytest.approx((28 * 1000 + 500) / (3 * 29))
    assert summary["offsets"]["observer"]["idle_blocks"] == 10


def test_relay_samples_of_the_p2p_relay_sample_logger_match_across_nodes(tmp_path: Path) -> None:
    summary = report.summarize(Run.load(_make_run(tmp_path / "chain-a-1", "sample")))

    assert summary["relay"]["relay1"]["median_ms"] == pytest.approx(51)
    assert summary["relay"]["witness"]["median_ms"] == pytest.approx(80)
    assert summary["relay"]["witness"]["n"] == 25
    assert summary["relay_coverage"] == {"nodes": 3, "sampled": 25, "on_every_node": 25}
    assert "25 on every one of them" in report.format_summary(summary)


def test_a_run_of_a_build_without_the_p2p_loggers_reports_no_counters(tmp_path: Path) -> None:
    run_dir = _make_run(tmp_path / "chain-a-1")
    (run_dir / "generated" / "WitnessNode0" / "logs" / "p2p" / "p2p.log").write_text("")
    meta = json.loads((run_dir / "out.json").read_text())
    del meta["p2p_loggers"]  # recorded by drivers that know the loggers only
    (run_dir / "out.json").write_text(json.dumps(meta))

    summary = report.summarize(Run.load(run_dir))

    assert summary["p2p_counters"] == {}
    assert summary["relay"]["relay1"]["median_ms"] == pytest.approx(51)
    assert "p2p_stats enabled on no node" in report.format_summary(summary)


def test_p2p_loggers_are_enabled_only_where_the_binary_has_them(tmp_path: Path) -> None:
    current, older = tmp_path / "hived-current", tmp_path / "hived-older"
    current.write_bytes(b"\x7fELF\0p2p\0p2p_stats\0relay sample\0p2p_relay_sample\0sync\0")
    older.write_bytes(b"\x7fELF\0p2p\0sync\0p2p_stats_unrelated\0")
    assert p2p_loggers.binary_loggers(current) == ["p2p_stats", "p2p_relay_sample"]
    assert p2p_loggers.binary_loggers(older) == []

    default = '{"name":"default","level":"info","appender":"stderr"} {"name":"p2p","level":"info","appender":"p2p"}'
    config = tmp_path / "config.ini"
    config.write_text(
        f"p2p-endpoint = 127.0.0.1:2001\nlog-logger = {p2p_loggers.with_loggers(default, ['p2p_stats'])}\n"
    )
    p2p_loggers.verify_config(config, ["p2p_stats"])
    with pytest.raises(p2p_loggers.P2pLoggersError):
        p2p_loggers.verify_config(config, ["p2p_stats", "p2p_relay_sample"])
    config.write_text(f"log-logger = {default}\n")
    p2p_loggers.verify_config(config, [])


def test_comparison_groups_runs_by_label(tmp_path: Path) -> None:
    first = report.summarize(Run.load(_make_run(tmp_path / "a1")))
    second = dict(report.summarize(Run.load(_make_run(tmp_path / "b1"))), label="b")

    table = report.format_comparison([first, second])

    header, *rows = table.splitlines()
    assert header.split() == ["metric", "a", "b"]
    assert any(row.startswith("relay1 relay median ms") and row.count("51.0") == 2 for row in rows)
    assert any(row.startswith("witness fetch examined/requested") and row.count("1.0") == 2 for row in rows)
    assert any(row.startswith("witness send messages/write") and row.count("2.5") == 2 for row in rows)


def _edit_meta(run_dir: Path, **changes: Any) -> None:
    meta = json.loads((run_dir / "out.json").read_text())
    (run_dir / "out.json").write_text(json.dumps({**meta, **changes}))


@pytest.mark.parametrize(
    ("changes", "short", "late_start"),
    [
        ({}, False, False),  # recorded before the window was checked: no expectation
        ({"blocks": 31, "colony_start_block": 109}, False, False),  # 29 blocks measured after the 2-block ramp
        ({"blocks": 40, "colony_start_block": 109}, True, False),  # the colonies loaded fewer blocks than asked
        ({"blocks": 31, "colony_start_block": 100}, True, True),  # the witness was at the start block: late start
    ],
)
def test_a_short_loaded_window_is_flagged(
    tmp_path: Path, changes: dict[str, Any], short: bool, late_start: bool
) -> None:
    run_dir = _make_run(tmp_path / "chain-a-1")
    _edit_meta(run_dir, **changes)

    summary = report.summarize(Run.load(run_dir))

    window = summary["window"]
    assert window["loaded_blocks"] == 29
    assert window["short"] is short
    assert window["late_start"] is late_start
    text = report.format_summary(summary)
    assert ("SHORT WINDOW" in text) is short
    assert ("started late" in text) is late_start


def _with_host_ticks(run_dir: Path, host_busy: list[int], own: list[int]) -> Path:
    """Samples every 30 s from head 105 (before the loaded window) on, with the given host and driver tree ticks."""
    samples = [
        {"t": 1000.0 + 30 * i, "head": 105 + 10 * i, "ticks": {}, "host_busy": busy, "own": tree}
        for i, (busy, tree) in enumerate(zip(host_busy, own, strict=True))
    ]
    _edit_meta(run_dir, samples=samples, nproc=48)
    return run_dir


# tck 100, 30 s per sample: 3000 ticks are one core; the driver's tree uses 2 cores; what the host did before the
# window (head 105 -> 115) is left out
_QUIET_HOST = [0, 900_000, 909_000, 918_000]  # 3 cores busy: 1 outside the run
_BUSY_HOST = [0, 900_000, 909_000, 942_000]  # then 11: 9 outside the run, 5 on average over the window
_OWN = [0, 6000, 12_000, 18_000]


@pytest.mark.parametrize(
    ("host_busy", "other", "peak", "busy"),
    [(_QUIET_HOST, 1.0, 1.0, False), (_BUSY_HOST, 5.0, 9.0, True)],
)
def test_cores_busy_outside_the_run_over_the_loaded_window_flag_a_busy_host(
    tmp_path: Path, host_busy: list[int], other: float, peak: float, busy: bool
) -> None:
    run_dir = _with_host_ticks(_make_run(tmp_path / "chain-a-1"), host_busy, _OWN)

    summary = report.summarize(Run.load(run_dir))

    load = summary["host_load"]
    assert load["other_busy_cores"] == pytest.approx(other)
    assert load["max_other_busy_cores"] == pytest.approx(peak)
    assert load["run_cores"] == pytest.approx(2.0)
    assert load["host_busy_cores"] == pytest.approx(other + 2.0)
    assert load["nproc"] == 48
    assert load["seconds"] == pytest.approx(60)
    assert load["busy"] is busy
    text = report.format_summary(summary)
    assert f"host: {other:.1f} cores busy outside the run (max {peak:.1f}" in text
    assert ("BUSY HOST" in text) is busy


def test_the_busy_host_threshold_is_an_option_of_report(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    run_dir = _with_host_ticks(_make_run(tmp_path / "chain-a-1"), _BUSY_HOST, _OWN)

    assert cli.main(["report", str(run_dir)]) == 0
    assert "BUSY HOST: 5.0 cores busy outside the run, over 4;" in capsys.readouterr().out
    assert cli.main(["report", "--busy-cores", "6", str(run_dir)]) == 0
    assert "BUSY HOST" not in capsys.readouterr().out
    assert cli.main(["report", "--json", str(run_dir)]) == 0
    load = json.loads(capsys.readouterr().out)["host_load"]
    assert load["busy"] is True
    assert load["other_busy_cores"] == pytest.approx(5.0)


def test_a_run_recorded_without_host_ticks_is_never_busy(tmp_path: Path) -> None:
    summary = report.summarize(Run.load(_make_run(tmp_path / "chain-a-1")), busy_cores=0)

    assert summary["host_load"] is None
    text = report.format_summary(summary)
    assert "BUSY HOST" not in text
    assert "host: no host CPU in the samples" in text


def test_comparison_counts_busy_runs_and_gives_the_mean_without_them(tmp_path: Path) -> None:
    summaries = [
        dict(report.summarize(Run.load(_with_host_ticks(_make_run(tmp_path / name), host, _OWN))), label=label)
        for name, label, host in (("a1", "a", _QUIET_HOST), ("a2", "a", _BUSY_HOST), ("b1", "b", _QUIET_HOST))
    ]
    summaries.append(dict(report.summarize(Run.load(_make_run(tmp_path / "b2"))), label="b"))  # no host ticks

    rows = {line[:40].strip(): line[40:].split() for line in report.format_comparison(summaries).splitlines()}

    assert rows["busy host runs"] == ["0.5", "quiet", "0.0", "(0/1*)", "0.0", "quiet", "0.0", "(0/0)"]
    assert rows["cores busy outside the run"] == ["3.0", "quiet", "1.0", "(1/5*)", "1.0", "quiet", "1.0", "(1)"]
    assert rows["observer corrected offset median ms"][:3] == ["450.0", "quiet", "450.0"]


def test_host_and_process_tree_ticks_from_proc(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    # user nice system idle iowait irq softirq steal guest guest_nice
    (tmp_path / "stat").write_text(
        "cpu  100 20 30 5000 400 6 7 8 90 9\ncpu0 50 10 15 2500 200 3 3 4 45 4\ncpu1 50 10 15 2500 200 3 4 4 45 5\n"
        "intr 12345\n"
    )
    # the driver (10), a node it started (11) and that node's child (12); 13 is not of the run
    for pid, ppid in ((10, 1), (11, 10), (12, 11), (13, 1)):
        (tmp_path / str(pid)).mkdir()
        fields = ["S", str(ppid), *["0"] * 9, str(pid), "1", "2", "3", *["0"] * 30]
        (tmp_path / str(pid) / "stat").write_text(f"{pid} (hived x) {' '.join(fields)}\n")
    monkeypatch.setattr(procfs, "_PROC", tmp_path)

    assert procfs.host_busy_ticks() == 100 + 20 + 30 + 6 + 7 + 8
    assert procfs.cpu_count() == 2
    assert procfs.tree_ticks(10) == (10 + 6) + (11 + 6) + (12 + 6)
    assert procfs.tree_ticks(13) == 13 + 6


def test_a_series_counts_an_unreportable_run_as_failed_and_compares_the_rest(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    block_log = tmp_path / "block_log"
    block_log.mkdir()
    (block_log / "alternate-chain-spec.json").write_text("{}")
    builds = []
    for label in ("a", "b"):
        hived = tmp_path / f"hived-{label}"
        hived.write_text("#!/bin/sh\n")
        hived.chmod(0o755)
        builds += ["--build", f"{label}={hived}"]
    executed = []

    def execute_run(spec: RunSpec, *_: object) -> int:
        executed.append(spec.name)
        run_dir = _make_run(tmp_path / "runs" / spec.name)
        # the colonies of s-b-1 started after the measured window: the witness produced no loaded block
        _edit_meta(run_dir, name=spec.name, label=spec.label, **({"first": 139} if spec.name == "s-b-1" else {}))
        return 0

    monkeypatch.setattr(cli, "execute_run", execute_run)
    monkeypatch.setattr(cli, "find_tools", lambda *_: {})
    monkeypatch.setattr(cli, "HostLock", contextlib.nullcontext)

    exit_code = cli.main([
        *("ab", "s", "--block-log", str(block_log), "--out-root", str(tmp_path / "runs"), "--tpb", "1000"),
        *("--image", "test-image", "--reps", "2", *builds),
    ])

    assert exit_code == 1
    assert executed == ["s-a-1", "s-b-1", "s-a-2", "s-b-2"]
    out, err = capsys.readouterr()
    assert "s-b-1: no report: s-b-1: the witness produced no loaded blocks after the ramp" in err
    assert [line.split()[1] for line in out.splitlines() if line.startswith("== ")] == ["s-a-1", "s-a-2", "s-b-2"]
    loaded_row = next(line for line in out.splitlines() if line.startswith("loaded blocks"))
    assert loaded_row.split()[2:] == ["29.0", "(29/29)", "29.0", "(29)"]


def _spec(**changes: Any) -> RunSpec:
    fields: dict[str, Any] = {
        "name": "r",
        "host": "h",
        "label": "",
        "out_dir": "/o",
        "block_log": "/b",
        "hived": "/h",
        "tpb": 10,
        "blocks": 5,
        "topology": "strict",
    }
    return RunSpec(**{**fields, **changes})


def test_topologies_wire_every_node_by_role() -> None:
    star = topology.wire(_spec(topology="strict", colonies=2))
    chain = topology.wire(_spec(topology="chain", relays=2))

    assert star.links == [("witness", "colony0"), ("witness", "colony1"), ("observer", "witness")]
    assert star.max_connections == {"observer": 1, "colony0": 1, "colony1": 1}
    assert chain.links == [
        ("relay1", "colony0"),
        ("relay2", "relay1"),
        ("witness", "relay2"),
        ("observer", "witness"),
    ]
    assert chain.max_connections == {"colony0": 1, "relay1": 2, "relay2": 2, "witness": 2, "observer": 1}


def test_a_two_host_run_splits_its_roles_between_the_hosts() -> None:
    spec = _spec(topology="chain", relays=2, remote_host="steem-2", remote_roles=["colony0", "relay1"])

    assert spec.roles_on("local") == ["witness", "observer", "relay2"]
    assert spec.roles_on("remote") == ["colony0", "relay1"]
    assert _spec().roles_on("local") == ["witness", "observer", "colony0"]
    assert _spec().roles_on("remote") == []


def test_clock_offset_comes_from_the_probe_with_the_shortest_round_trip() -> None:
    # the remote clock is 2 ms ahead; the slow probes' replies were delayed on the way back, which would bias them
    probes = [(10.0, 10.0035, 10.004), (20.0, 20.0021, 20.0002), (30.0, 30.0025, 30.003)]

    clock = remote.clock_offset(probes)

    assert clock["offset_s"] == pytest.approx(0.002)
    assert clock["uncertainty_s"] == pytest.approx(0.0001)
    assert clock["min_rtt_s"] == pytest.approx(0.0002)
    assert clock["median_rtt_s"] == pytest.approx(0.003)
    assert clock["probes"] == 3


def test_two_host_ports_come_from_below_the_ephemeral_range(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    port_range = tmp_path / "ip_local_port_range"
    port_range.write_text("32768\t60999\n")
    monkeypatch.setattr(harness, "_EPHEMERAL_PORTS", port_range)
    with contextlib.ExitStack() as stack:
        busy = stack.enter_context(socket.socket())
        busy.bind(("0.0.0.0", harness.free_ports(1)[0]))
        busy.listen()
        busy_port = busy.getsockname()[1]
        monkeypatch.setattr(harness, "_P2P_PORT_BASE", busy_port)

        ports = harness.free_ports(3)

    assert len(set(ports)) == 3
    assert all(busy_port < port < 32768 for port in ports)


_TIMESYNC_STATUS = """       Server: 192.168.3.1 (192.168.3.1)
Poll interval: 34min 8s (min: 32s; max 34min 8s)
      Stratum: 3
Root distance: 41.022ms (max: 5s)
       Offset: -94us
        Delay: 264us
"""


@pytest.mark.parametrize(
    ("text", "source", "ahead"),
    [
        (
            "Reference ID    : 25E7CA68 (ntp-nts-3.ps6.canonical.com)\nSystem time     : 0.000852 seconds slow of NTP time\n",
            "chrony",
            -0.000852,
        ),
        (
            "Reference ID    : 5BBD5B70 (ntp)\nSystem time     : 0.000684133 seconds fast of NTP time\n",
            "chrony",
            0.000684133,
        ),
        # timesyncd's offset is the server's clock minus this one's: -94 us means this clock is 94 us ahead
        ("sh: chronyc: not found\n" + _TIMESYNC_STATUS, "timesyncd", 0.000094),
        ("506 Cannot talk to daemon\nSystem has not been booted with systemd\n", None, None),
    ],
)
def test_ntp_status_tells_how_far_the_clock_is_from_ntp_time(
    text: str, source: str | None, ahead: float | None
) -> None:
    status = remote.ntp_status(text)

    assert status["source"] == source
    assert status["ahead_of_ntp_s"] == (None if ahead is None else pytest.approx(ahead))
    assert (status["reference"] is None) is (ahead is None)


def _make_two_host_run(directory: Path, offset_s: float) -> Path:
    """_make_run with colony0 on a remote host whose clock is `offset_s` ahead: its timestamps carry that offset."""
    run_dir = _make_run(directory, "sample")
    remote_generated = run_dir / "remote" / "generated"
    remote_generated.mkdir(parents=True)
    shutil.move(run_dir / "generated" / "ApiNode1", remote_generated / "ApiNode1")
    colony_log = remote_generated / "ApiNode1" / "logs" / "p2p" / "p2p.log"
    shift_us = round(offset_s * 1e6)
    colony_log.write_text("".join(_relay_line("sample", "BRD", i, 1_000_000 + shift_us + i * 1000) for i in range(25)))
    meta = json.loads((run_dir / "out.json").read_text())
    colony_ticks = [{"7": ["colony", 0]}, {"7": ["colony", 1500]}]
    remote_out = {
        **meta,
        "roles": {"colony0": "ApiNode1"},
        "p2p_parameters": {"given": {}, "applied": {"colony0": {}}, "unsupported": {}},
        "p2p_loggers": {"colony0": list(p2p_loggers.LOGGERS)},
        "samples": [
            {"t": 2000.0 + 30 * i, "head": 115 + 10 * i, "ticks": {"colony0": colony_ticks[i]}} for i in (0, 1)
        ],
        "seed_links_s": 0.0,
        "container_host": "steem-2",
        "pids": {"colony0": 7},
        "links": [],
    }
    meta.update(
        roles={role: name for role, name in meta["roles"].items() if role != "colony0"},
        p2p_parameters={"given": {}, "applied": {}, "unsupported": {}},
        seed_links_s=1.5,
        pids={},
        links=[],
        container_host="steem-4",
        remote_host="steem-2",
        remote_roles=["colony0"],
    )
    clock = {
        "offset_s": offset_s,
        "uncertainty_s": 0.0001,
        "min_rtt_s": 0.0002,
        "median_rtt_s": 0.0003,
        "probes": 50,
        "ntp_offset_s": 0.0015,
    }
    (run_dir / "out.json").write_text(json.dumps(remote.merge_remote_part(meta, remote_out, clock)))
    return run_dir


def test_a_two_host_run_reports_relay_latency_on_one_clock(tmp_path: Path) -> None:
    summary = report.summarize(Run.load(_make_two_host_run(tmp_path / "chain-a-1", offset_s=0.005)))

    # colony0's timestamps are 5 ms ahead; corrected, sampled transactions reach relay1 51 ms after it broadcast them
    assert summary["relay"]["relay1"]["median_ms"] == pytest.approx(51)
    assert summary["relay"]["witness"]["median_ms"] == pytest.approx(80)
    assert summary["relay_coverage"] == {"nodes": 3, "sampled": 25, "on_every_node": 25}
    assert summary["thread_cpu"]["colony0"]["TOTAL"] == pytest.approx(50)
    assert summary["thread_cpu"]["witness"]["TOTAL"] == pytest.approx(30)
    placement = summary["placement"]
    assert placement["local"] == {"host": "steem-x", "roles": ["witness", "observer", "relay1"]}
    assert placement["remote"] == {"host": "steem-2", "roles": ["colony0"]}
    assert placement["clock"]["offset_s"] == pytest.approx(0.005)
    assert "two hosts: steem-x (witness,observer,relay1) and steem-2 (colony0); remote clock +5.000 ms" in (
        report.format_summary(summary)
    )
    assert report.summarize(Run.load(_make_run(tmp_path / "single")))["placement"] is None


# Stand-ins for the driver container, one per host: each writes its nodes' logs, says it is ready, waits for the
# start signal and (the remote one) for the local part's end, then writes its out.json.
_FAKE_DRIVER = """
import json, os, sys, time
from pathlib import Path
out_dir, tmp_dir, part, roles = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], sys.argv[4:]
if os.environ.get("P2P_LOAD_TEST_FAILING_PART") == part:
    sys.exit(3)
for role in roles:
    (tmp_dir / "generated" / f"node-{role}").mkdir(parents=True)
    (tmp_dir / "generated" / f"node-{role}" / "stderr_0.log").write_text(part)
(out_dir / f"{part}.ready").write_text("{}")
while not (out_dir / f"{part}.go").is_file():
    time.sleep(0.02)
start = json.loads((out_dir / f"{part}.go").read_text())["t0"]
while part == "remote" and not (out_dir / "remote.stop").is_file():
    time.sleep(0.02)
out = {
    "start": start, "roles": {role: f"node-{role}" for role in roles},
    "p2p_parameters": {"given": {}, "applied": {role: {} for role in roles}, "unsupported": {}},
    "p2p_loggers": {role: [] for role in roles}, "samples": [{"t": start, "head": 1, "ticks": {}}],
    "seed_links_s": 1.0 if part == "local" else 2.0, "container_host": part, "pids": {}, "links": [],
    "first": 1, "last": 2,
}
(out_dir / "out.json").write_text(json.dumps(out))
"""


def _fake_start_driver(
    spec: RunSpec, image: str, tmp_dir: Path, tools: dict[str, Path], part: str
) -> subprocess.Popen[bytes]:
    return subprocess.Popen([
        sys.executable,
        "-c",
        _FAKE_DRIVER,
        spec.out_dir,
        str(tmp_dir),
        part,
        *spec.roles_on(part),
    ])


def _fake_end_driver(spec: RunSpec, driver: subprocess.Popen[bytes]) -> None:
    if driver.poll() is None:
        driver.kill()
        driver.wait()


# the remote host's `p2p-load part`, with the fake driver and its own tmpfs root
_REMOTE_PART = """
import sys
from pathlib import Path
from python.functional.p2p_load import __main__ as cli, harness
from python.functional.p2p_load.test_p2p_load_harness import _fake_end_driver, _fake_start_driver
harness.TMPFS_ROOT = Path(sys.argv[1])
harness._start_driver, harness._end_driver = _fake_start_driver, _fake_end_driver
sys.exit(cli.main(["part"]))
"""


class _OtherHost(remote.RemoteHost):
    """A RemoteHost whose part runs here as a process, with its own run and tmpfs directories, 250 ms ahead."""

    def __init__(self, root: Path) -> None:
        super().__init__("steem-2")
        self._root = root

    def measure_clock(self) -> dict[str, Any]:
        return {"host": self.host, "offset_s": 0.25, "uncertainty_s": 0.0001}

    def start_part(self, request: dict[str, Any], log: Path) -> remote.RemotePart:
        out_dir = self._root / "runs" / request["spec"]["name"]
        request = {**request, "spec": {**request["spec"], "out_dir": str(out_dir)}}
        command = [sys.executable, "-c", _REMOTE_PART, str(self._root / "shm")]
        return remote.RemotePart(command, request, log)

    def fetch(self, remote_dir: Path, local_dir: Path) -> None:
        shutil.copytree(self._root / "runs" / remote_dir.name, local_dir)


@pytest.fixture
def two_hosts(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> tuple[RunSpec, _OtherHost]:
    monkeypatch.setenv("PYTHONPATH", os.pathsep.join(sys.path))  # the remote part imports what this test does
    monkeypatch.setattr(harness, "TMPFS_ROOT", tmp_path / "shm")
    monkeypatch.setattr(harness, "_START_LEAD_S", 0.1)
    monkeypatch.setattr(harness, "_start_driver", _fake_start_driver)
    monkeypatch.setattr(harness, "_end_driver", _fake_end_driver)
    spec = _spec(
        name="split-1",
        out_dir=str(tmp_path / "runs" / "split-1"),
        colonies=2,
        remote_host="steem-2",
        remote_roles=["colony0", "colony1"],
        endpoints={"witness": "10.0.0.4:2001", "colony0": "10.0.0.2:2002", "colony1": "10.0.0.2:2003"},
    )
    return spec, _OtherHost(tmp_path / "steem-2")


def test_a_two_host_run_starts_both_hosts_together_and_keeps_both_hosts_logs(
    tmp_path: Path, two_hosts: tuple[RunSpec, _OtherHost]
) -> None:
    spec, other_host = two_hosts

    exit_code = harness.execute_run(spec, tmp_path / "runs", "image", {}, remote=other_host)

    assert exit_code == 0
    run_dir = Path(spec.out_dir)
    local_start = json.loads((run_dir / "local.go").read_text())["t0"]
    remote_start = json.loads((run_dir / "remote" / "remote.go").read_text())["t0"]
    assert remote_start - local_start == pytest.approx(0.25)  # one moment, on each host's own clock
    assert (run_dir / "remote" / "remote.stop").is_file()  # the remote part ran until the local one ended
    assert (run_dir / "generated" / "node-witness" / "stderr_0.log").read_text() == "local"
    assert (run_dir / "remote" / "generated" / "node-colony1" / "stderr_0.log").read_text() == "remote"
    out = json.loads((run_dir / "out.json").read_text())
    assert out["roles"] == {
        "witness": "node-witness",
        "observer": "node-observer",
        "colony0": "node-colony0",
        "colony1": "node-colony1",
    }
    assert out["clock"]["offset_s"] == pytest.approx(0.25)
    assert out["seed_links_s"] == pytest.approx(2.0)
    assert out["remote"]["container_host"] == "remote"
    assert len(out["samples"]) == 2
    assert not any((tmp_path / "shm").iterdir())
    assert not any((tmp_path / "steem-2" / "shm").iterdir())


def test_a_remote_part_failing_before_the_start_fails_the_run(
    tmp_path: Path, two_hosts: tuple[RunSpec, _OtherHost], monkeypatch: pytest.MonkeyPatch
) -> None:
    spec, other_host = two_hosts
    monkeypatch.setenv("P2P_LOAD_TEST_FAILING_PART", "remote")

    exit_code = harness.execute_run(spec, tmp_path / "runs", "image", {}, remote=other_host)

    assert exit_code == 3
    run_dir = Path(spec.out_dir)
    assert not (run_dir / "local.go").exists()  # neither host's nodes were started
    assert not (run_dir / "out.json").exists()
    assert "end split-1 exit=3" in (tmp_path / "runs" / "series.log").read_text()
