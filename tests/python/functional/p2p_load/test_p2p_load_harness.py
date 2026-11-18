from __future__ import annotations

import contextlib
import json
from datetime import UTC, datetime
from typing import TYPE_CHECKING, Any

import pytest

import test_tools as tt
from python.functional.p2p_load import __main__ as cli
from python.functional.p2p_load import p2p_loggers, p2p_parameters, report
from python.functional.p2p_load.analysis import Run
from python.functional.p2p_load.host_lock import HarnessBusyError, HostLock
from python.functional.p2p_load.spec import RunSpec

if TYPE_CHECKING:
    from pathlib import Path

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


@pytest.mark.parametrize(
    ("changes", "error"),
    [
        ({"topology": "ring"}, "topology"),
        ({"topology": "chain", "colonies": 2}, "one colony"),
        ({"topology": "star", "relays": 1}, "relays"),
        ({"node_hived": {"relay3": "/bin/true"}}, "roles"),
        ({"colony_ports": {"1": 4000}}, "colony index"),
    ],
)
def test_spec_validation(changes: dict[str, Any], error: str) -> None:
    spec = RunSpec(
        name="r",
        host="h",
        label="",
        out_dir="/o",
        block_log="/b",
        hived="/h",
        tpb=10,
        blocks=5,
        topology="strict",
        **changes,
    )
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
