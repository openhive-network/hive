"""Command line of the p2p load harness: run, ab, report, compare (and drive, which the run's container calls)."""

from __future__ import annotations

import argparse
import json
import os
import socket
import sys
from pathlib import Path
from typing import Any

from python.functional.p2p_load import analysis, report
from python.functional.p2p_load.harness import HarnessSetupError, default_image, execute_run, find_tools, free_port
from python.functional.p2p_load.host_lock import HarnessBusyError, HostLock
from python.functional.p2p_load.spec import TOPOLOGIES, RunSpec

# shorthand flags for the p2p-parameters keys the #877 measurements vary; --p2p-parameter sets any other
_P2P_FLAGS = {
    "flush_us": "transaction_flush_delay_microseconds",
    "fetch_us": "fetch_items_minimum_interval_microseconds",
    "inflight": "maximum_transactions_in_flight",
}


def _key_value(text: str) -> tuple[str, str]:
    key, sep, value = text.partition("=")
    if not sep or not key or not value:
        raise argparse.ArgumentTypeError(f"expected KEY=VALUE, got {text!r}")
    return key, value


def _executable(text: str) -> Path:
    path = Path(text).resolve()
    if not (path.is_file() and os.access(path, os.X_OK)):
        raise HarnessSetupError(f"{text} is not an executable file")
    return path


def _add_run_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("name", help="run name (the A/B series appends -LABEL-REP); its directory must not exist yet")
    parser.add_argument("--block-log", type=Path, required=True, help="single_sign_universal_block_log directory")
    parser.add_argument("--out-root", type=Path, default=Path("p2p-load-runs"), help="where run directories go")
    parser.add_argument("--topology", choices=TOPOLOGIES, default="strict")
    parser.add_argument("--tpb", type=int, required=True, help="colony transactions per block, split over colonies")
    parser.add_argument("--blocks", type=int, default=30, help="loaded blocks measured, from the colonies' start")
    parser.add_argument("--colonies", type=int, default=1)
    parser.add_argument("--relays", type=int, default=0, help="relays between colony0 and the witness (chain)")
    parser.add_argument(
        "--node-hived",
        type=_key_value,
        action="append",
        default=[],
        metavar="ROLE=PATH",
        help="hived for one node (witness, observer, colonyN, relayN); mixed-version runs",
    )
    parser.add_argument("--tools", type=Path, help="get_dev_key / block_log_util (flat dir or build tree)")
    for flag, key in _P2P_FLAGS.items():
        parser.add_argument(f"--{flag.replace('_', '-')}", type=int, help=f"sets {key}; unset = binary default")
    parser.add_argument(
        "--p2p-parameter",
        type=_key_value,
        action="append",
        default=[],
        metavar="KEY=JSON",
        help="sets any p2p-parameters key on every node whose binary has it",
    )
    parser.add_argument(
        "--late-observer",
        type=int,
        default=0,
        metavar="K",
        help="start the observer K blocks after the colonies start loading",
    )
    parser.add_argument("--netem-ms", type=int, default=0, help="tc netem delay, each way, on the chosen colony links")
    parser.add_argument("--netem-colonies", default="0", help="comma-separated colony indexes --netem-ms delays")
    parser.add_argument(
        "--sends",
        type=int,
        default=0,
        metavar="SECONDS",
        help="count the witness's send syscalls with bpftrace for SECONDS once loaded",
    )
    parser.add_argument("--colony-start-delay", type=int, default=30, help="idle blocks before the colonies start")
    parser.add_argument("--mix", choices=("default", "large"), default="default", help="colony transaction mix")
    parser.add_argument(
        "--no-p2p-loggers",
        dest="p2p_loggers",
        action="store_false",
        help="do not enable the p2p_stats and p2p_relay_sample loggers, even where the binary has them",
    )
    parser.add_argument("--image", help="test-tools image (default: P2P_LOAD_IMAGE or .aidev/project.yaml's)")


def _p2p_parameters(args: argparse.Namespace) -> dict[str, Any]:
    given: dict[str, Any] = {}
    for key, value in args.p2p_parameter:
        try:
            given[key] = json.loads(value)
        except json.JSONDecodeError as error:
            raise HarnessSetupError(f"--p2p-parameter {key}: {value!r} is not JSON") from error
    for flag, key in _P2P_FLAGS.items():
        if getattr(args, flag) is not None:
            given[key] = getattr(args, flag)
    return given


def _spec(args: argparse.Namespace, name: str, hived: Path, label: str) -> RunSpec:
    if not (args.block_log / "alternate-chain-spec.json").is_file():
        raise HarnessSetupError(f"{args.block_log} has no alternate-chain-spec.json; is it a universal block log?")
    netem_colonies = [int(i) for i in args.netem_colonies.split(",") if i] if args.netem_ms else []
    spec = RunSpec(
        name=name,
        host=socket.gethostname(),
        label=label,
        out_dir=str((args.out_root / name).resolve()),
        block_log=str(args.block_log.resolve()),
        hived=str(hived),
        tpb=args.tpb,
        blocks=args.blocks,
        topology=args.topology,
        colonies=args.colonies,
        relays=args.relays,
        node_hived={role: str(_executable(path)) for role, path in args.node_hived},
        p2p_parameters=_p2p_parameters(args),
        late_observer_blocks=args.late_observer,
        colony_ports={str(i): free_port() for i in netem_colonies},
        netem_ms=args.netem_ms,
        colony_start_delay=args.colony_start_delay,
        mix=args.mix,
        p2p_loggers=args.p2p_loggers,
    )
    try:
        spec.validate()
    except ValueError as error:
        raise HarnessSetupError(str(error)) from error
    return spec


def _series(args: argparse.Namespace, builds: list[tuple[str, Path]], reps: int) -> int:
    """Run every build `reps` times, alternating builds, under one host lock; print the reports."""
    args.out_root.mkdir(parents=True, exist_ok=True)
    image = args.image or default_image()
    tools = find_tools(args.tools, builds[0][1])
    finished: list[dict[str, Any]] = []
    failed = 0
    with HostLock():
        for rep in range(1, reps + 1):
            for label, hived in builds:
                name = f"{args.name}-{label}-{rep}" if len(builds) > 1 or reps > 1 else args.name
                spec = _spec(args, name, hived, label)
                exit_code = execute_run(spec, args.out_root, image, tools, args.sends)
                if exit_code != 0:
                    failed += 1
                    print(f"{name}: driver exited {exit_code}; see {spec.out_dir}/driver.log", file=sys.stderr)
                    continue
                try:
                    summary = report.summarize(analysis.Run.load(Path(spec.out_dir)))
                except analysis.RunDataError as error:
                    failed += 1
                    print(f"{name}: no report: {error}", file=sys.stderr)
                    continue
                finished.append(summary)
                print(report.format_summary(summary), flush=True)
    if len(builds) > 1 and finished:
        print(report.format_comparison(finished))
    return 1 if failed else 0


def _run(args: argparse.Namespace) -> int:
    hived = _executable(args.hived)
    return _series(args, [(args.label, hived)], args.reps)


def _ab(args: argparse.Namespace) -> int:
    builds = [(label, _executable(path)) for label, path in args.build]
    if len(builds) < 2 or len({label for label, _ in builds}) != len(builds):
        raise HarnessSetupError("--build needs at least two distinct labels")
    if args.node_hived:
        raise HarnessSetupError("--node-hived and --build do not combine: each A/B run uses one build on every node")
    return _series(args, builds, args.reps)


def _summaries(args: argparse.Namespace) -> list[dict[str, Any]]:
    return [
        report.summarize(analysis.Run.load(path), args.full_txs, args.window_blocks, args.busy_cores)
        for path in args.run_dirs
    ]


def _report(args: argparse.Namespace) -> int:
    for summary in _summaries(args):
        print(json.dumps(summary) if args.json else report.format_summary(summary))
    return 0


def _compare(args: argparse.Namespace) -> int:
    print(report.format_comparison(_summaries(args)))
    return 0


def _drive(args: argparse.Namespace) -> int:
    # imported here: it needs test-tools, which only the run's container has
    from python.functional.p2p_load.driver import main

    main(args.spec)
    return 0


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="p2p-load", description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    run = commands.add_parser("run", help="one run (or --reps runs) of one build")
    _add_run_options(run)
    run.add_argument("--hived", required=True, help="hived for every node not named by --node-hived")
    run.add_argument("--label", default="", help="label recorded in out.json (grouping for compare)")
    run.add_argument("--reps", type=int, default=1)
    run.set_defaults(handler=_run)

    ab = commands.add_parser("ab", help="alternate two or more builds on this host, then compare them")
    _add_run_options(ab)
    ab.add_argument("--build", type=_key_value, action="append", required=True, metavar="LABEL=PATH")
    ab.add_argument("--reps", type=int, default=2)
    ab.set_defaults(handler=_ab)

    for command, handler, text in (
        ("report", _report, "metrics of finished runs"),
        ("compare", _compare, "headline metrics of finished runs per label"),
    ):
        sub = commands.add_parser(command, help=text)
        sub.add_argument("run_dirs", type=Path, nargs="+")
        sub.add_argument(
            "--full-txs", type=int, help="blocks with fewer transactions are underfilled (default 90%% of tpb)"
        )
        sub.add_argument("--window-blocks", type=int, default=10, help="blocks per tx/s window")
        sub.add_argument(
            "--busy-cores",
            type=float,
            default=analysis.BUSY_CORES,
            help="flag BUSY HOST above this many cores busy outside the run (default %(default)s)",
        )
        sub.set_defaults(handler=handler)
    commands.choices["report"].add_argument("--json", action="store_true")

    drive = commands.add_parser("drive", help="(inside the run's container) execute a spec.json")
    drive.add_argument("spec", type=Path)
    drive.set_defaults(handler=_drive)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        return int(args.handler(args))
    except HarnessBusyError as error:
        print(f"not starting: {error}", file=sys.stderr)
        return 2
    except (HarnessSetupError, analysis.RunDataError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
