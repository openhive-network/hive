from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any

import pytest

import test_tools as tt
from python.functional.p2p_load import p2p_parameters, report
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


def _block_stats(num: int, txs: int, offset_us: int, arrival_us: int, inc: int, kind: str) -> str:
    stats = {
        "num": num,
        "type": kind,
        "txs": txs,
        "offset": offset_us,
        "before": {"inc": inc},
        "exec": {"offset": arrival_us},
    }
    return f"2026-11-18T06:00:00.000 info Block stats:{json.dumps(stats)}\n"


def _make_run(directory: Path) -> Path:
    """
    A finished chain run: blocks 100-109 idle, 110-140 loaded with 1000 tx (block 120 only 500).

    The observer's clock is 20 ms ahead of the witness's and blocks reach it 50 ms after the witness is ready;
    sampled transactions reach relay1 51 ms after colony0 broadcast them.
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
    colony_log = "".join(f"RLY BRD id={i:016x} t={1_000_000 + i * 1000}\n" for i in range(25))
    relay_log = "".join(f"RLY RCV id={i:016x} t={1_051_000 + i * 1000}\n" for i in range(25))
    (generated / "ApiNode1" / "logs" / "p2p" / "p2p.log").write_text(colony_log)
    (generated / "ApiNode2" / "logs" / "p2p" / "p2p.log").write_text(relay_log)

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


def test_report_of_a_finished_run(tmp_path: Path) -> None:
    summary = report.summarize(Run.load(_make_run(tmp_path / "chain-a-1")))

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


def test_comparison_groups_runs_by_label(tmp_path: Path) -> None:
    first = report.summarize(Run.load(_make_run(tmp_path / "a1")))
    second = dict(report.summarize(Run.load(_make_run(tmp_path / "b1"))), label="b")

    table = report.format_comparison([first, second])

    header, *rows = table.splitlines()
    assert header.split() == ["metric", "a", "b"]
    assert any(row.startswith("relay1 relay median ms") and row.count("51.0") == 2 for row in rows)
