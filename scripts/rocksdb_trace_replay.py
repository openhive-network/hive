#!/usr/bin/env python3
"""Replays a RocksDB trace recorded by hived under several RocksDB configurations and compares them.

hived records a trace with comments-rocksdb-trace-dir / comments-rocksdb-trace-blocks; the trace
directory (<dir>/COMMENT) holds trace_info.json, the trace and a checkpoint of the database where the
trace starts. For every configuration file this script copies the checkpoint, rewrites the copy
under the configuration (`hive_db_bench -benchmarks=compact`, so its SST files have the configured
format rather than hived's), replays the trace on it with `hive_db_bench -benchmarks=replay` and
reports the replay time, the latency percentiles of the reads and writes, and the size of the
database afterwards. db_bench's compact rewrites the default column family, the one the comment
archive keeps its comments in.

A configuration file is a fragment of a RocksDB OPTIONS file, applied on top of the checkpoint's own
OPTIONS file (the options hived ran with):

    # format_version 7 and 32 KiB blocks
    [TableOptions/BlockBasedTable]
    format_version = 7
    block_size = 32768

    [db_bench]
    cache_size = 4294967296

A section without a column family name ([CFOptions], [TableOptions/BlockBasedTable]) applies to the
sections of every column family, one with a name ([CFOptions "default"]) to that one; [DBOptions]
to the database options. db_bench gives every column family the options of the first one
("default"). [db_bench] sets db_bench flags (-name=value); the block cache is one of them
(cache_size, cache_type), as db_bench replaces the one the OPTIONS file describes.

See doc/devs/rocksdb-trace-replay.md.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

# hived's block cache: comments-rocksdb-block-cache-size default, cache type, and the high priority
# pool of RocksDB's NewLRUCache (db_bench's own default is no pool)
DEFAULT_DB_BENCH_FLAGS = {
    "cache_type": "lru_cache",
    "cache_size": str(1024 * 1024 * 1024),
    "cache_high_pri_pool_ratio": "0.5",
    "trace_replay_threads": "1",
}

# db_bench histograms reported per configuration, by their RocksDB statistics names
HISTOGRAMS = {
    "get": "rocksdb.db.get.micros",
    "multiget": "rocksdb.db.multiget.micros",
    "write": "rocksdb.db.write.micros",
    "seek": "rocksdb.db.seek.micros",
}

LINKED_SUFFIXES = (".sst", ".blob")

_SECTION = re.compile(r'^\[(?P<kind>[^\]"]+?)(?:\s+"(?P<name>[^"]*)")?\]\s*$')
_HISTOGRAM = re.compile(
    r"^(?P<name>\S+) P50 : (?P<p50>[\d.]+) P95 : (?P<p95>[\d.]+) P99 : (?P<p99>[\d.]+)"
    r" P100 : (?P<p100>[\d.]+) COUNT : (?P<count>\d+)"
)
_BENCHMARK_SECONDS = r"^{}\s*:.*?([\d.]+) seconds"


class HarnessError(Exception):
    """A trace directory, configuration file or replay that cannot be used."""


@dataclass
class OptionsSection:
    kind: str
    name: str | None
    lines: list[str] = field(default_factory=list)

    def matches(self, kind: str, name: str | None) -> bool:
        return self.kind == kind and (name is None or self.name == name)

    def set(self, key: str, value: str) -> None:
        for i, line in enumerate(self.lines):
            if line.split("=", 1)[0].strip() == key:
                self.lines[i] = f"  {key}={value}"
                return
        self.lines.append(f"  {key}={value}")


@dataclass
class Configuration:
    name: str
    options: dict[tuple[str, str | None], dict[str, str]]
    db_bench_flags: dict[str, str]


@dataclass
class Trace:
    info: dict[str, Any]
    checkpoint: Path
    trace_file: Path
    options: list[OptionsSection]
    column_families: int


@dataclass
class Result:
    configuration: str
    ok: bool
    error: str | None = None
    rewrite_seconds: float | None = None
    replay_seconds: float | None = None
    wall_seconds: float | None = None
    latency_micros: dict[str, dict[str, float]] = field(default_factory=dict)
    db_bytes: int | None = None
    sst_bytes: int | None = None


def parse_ini(text: str, path: Path) -> list[tuple[str, str | None, list[tuple[str, str]]]]:
    sections: list[tuple[str, str | None, list[tuple[str, str]]]] = []
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        header = _SECTION.match(line)
        if header:
            sections.append((header["kind"].strip(), header["name"], []))
            continue
        if "=" not in line or not sections:
            raise HarnessError(f"{path}:{number}: expected [section] or key=value, got {raw!r}")
        key, value = line.split("=", 1)
        sections[-1][2].append((key.strip(), value.strip()))
    return sections


def load_configuration(path: Path) -> Configuration:
    options: dict[tuple[str, str | None], dict[str, str]] = {}
    flags: dict[str, str] = {}
    for kind, name, entries in parse_ini(path.read_text(), path):
        if kind == "db_bench":
            flags.update(entries)
        else:
            options.setdefault((kind, name), {}).update(entries)
    return Configuration(path.stem, options, flags)


def load_options_file(path: Path) -> list[OptionsSection]:
    sections: list[OptionsSection] = [OptionsSection("", None)]  # the comments before the first section
    for raw in path.read_text().splitlines():
        header = _SECTION.match(raw.strip())
        if header:
            sections.append(OptionsSection(header["kind"].strip(), header["name"]))
        else:
            sections[-1].lines.append(raw)
    return sections


def render_options(sections: list[OptionsSection]) -> str:
    out: list[str] = []
    for section in sections:
        if section.kind:
            out.append(f'[{section.kind} "{section.name}"]' if section.name is not None else f"[{section.kind}]")
        out.extend(section.lines)
    return "\n".join(out) + "\n"


def apply_configuration(base: list[OptionsSection], configuration: Configuration) -> list[OptionsSection]:
    sections = [OptionsSection(s.kind, s.name, list(s.lines)) for s in base]
    for (kind, name), entries in configuration.options.items():
        matching = [s for s in sections if s.matches(kind, name)]
        if not matching:
            raise HarnessError(
                f"{configuration.name}: no [{kind}] section for {name or 'any column family'} to apply to"
            )
        for section in matching:
            for key, value in entries.items():
                section.set(key, value)
    return sections


def latest_options_file(db_dir: Path) -> Path:
    candidates = [(int(m[1]), p) for p in db_dir.iterdir() if (m := re.fullmatch(r"OPTIONS-(\d+)", p.name))]
    if not candidates:
        raise HarnessError(f"{db_dir} holds no OPTIONS file")
    return max(candidates)[1]


def copy_database(source: Path, target: Path) -> None:
    """Copies a database, hard linking its SST and blob files, which RocksDB never changes in place."""
    target.mkdir(parents=True)
    for entry in source.iterdir():
        destination = target / entry.name
        if entry.name.endswith(LINKED_SUFFIXES):
            try:
                os.link(entry, destination)
                continue
            except OSError:
                pass
        shutil.copy2(entry, destination)


def database_size(db_dir: Path) -> tuple[int, int]:
    total = sst = 0
    for entry in db_dir.iterdir():
        if entry.is_file():
            size = entry.stat().st_size
            total += size
            if entry.name.endswith(".sst"):
                sst += size
    return total, sst


def drop_page_cache() -> None:
    os.sync()
    try:
        Path("/proc/sys/vm/drop_caches").write_text("3\n")
    except OSError as e:
        raise HarnessError(f"cannot drop the page cache (needs root): {e}") from e


def benchmark_seconds(output: str, benchmark: str) -> float | None:
    reported = re.search(_BENCHMARK_SECONDS.format(benchmark), output, re.MULTILINE)
    return float(reported[1]) if reported else None


def parse_replay_output(output: str, result: Result) -> None:
    result.replay_seconds = benchmark_seconds(output, "replay")
    wanted = {statistic: label for label, statistic in HISTOGRAMS.items()}
    for line in output.splitlines():
        histogram = _HISTOGRAM.match(line.strip())
        if histogram and histogram["name"] in wanted and int(histogram["count"]) > 0:
            result.latency_micros[wanted[histogram["name"]]] = {
                key: float(histogram[key]) for key in ("p50", "p95", "p99", "p100", "count")
            }


def run_db_bench(args: argparse.Namespace, flags: dict[str, str], log_file: Path) -> str:
    """Runs db_bench, returning its output; raises HarnessError when it fails."""
    command = [args.db_bench, *(f"-{key}={value}" for key, value in flags.items()), *args.db_bench_arg]
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    log_file.write_text(f"$ {' '.join(command)}\n{completed.stdout}\n--- stderr ---\n{completed.stderr}")
    benchmark = flags["benchmarks"]
    if completed.returncode != 0 or benchmark_seconds(completed.stdout, benchmark) is None:
        last_error = (completed.stderr.strip().splitlines() or ["no output"])[-1]
        raise HarnessError(f"db_bench {benchmark} failed (exit {completed.returncode}): {last_error}; see {log_file}")
    if benchmark == "replay" and "Replay completed" not in completed.stdout:
        raise HarnessError(f"db_bench did not complete the replay; see {log_file}")
    return completed.stdout


def replay(args: argparse.Namespace, configuration: Configuration, trace: Trace, run_dir: Path, result: Result) -> None:
    db_dir = run_dir / "db"
    options_file = run_dir / "OPTIONS.ini"
    copy_database(trace.checkpoint, db_dir)
    options_file.write_text(render_options(apply_configuration(trace.options, configuration)))
    flags = dict(DEFAULT_DB_BENCH_FLAGS)
    flags.update(configuration.db_bench_flags)
    flags.update({
        "use_existing_db": "true",
        "db": str(db_dir),
        "options_file": str(options_file),
        "num_column_families": str(trace.column_families),
    })
    if not args.no_rewrite:
        output = run_db_bench(args, {**flags, "benchmarks": "compact"}, run_dir / "db_bench-compact.log")
        result.rewrite_seconds = benchmark_seconds(output, "compact")
    if args.drop_caches:
        drop_page_cache()
    replay_flags = {
        **flags,
        "benchmarks": "replay",
        "trace_file": str(trace.trace_file),
        "trace_replay_fast_forward": "1e9",
        "statistics": "true",
    }
    started = time.monotonic()
    output = run_db_bench(args, replay_flags, run_dir / "db_bench.log")
    result.wall_seconds = round(time.monotonic() - started, 3)
    parse_replay_output(output, result)
    result.db_bytes, result.sst_bytes = database_size(db_dir)


def run_configuration(args: argparse.Namespace, configuration: Configuration, trace: Trace, work_dir: Path) -> Result:
    result = Result(configuration.name, ok=False)
    run_dir = work_dir / configuration.name
    if run_dir.exists():
        shutil.rmtree(run_dir)
    try:
        replay(args, configuration, trace, run_dir, result)
        result.ok = True
    except (HarnessError, OSError) as e:
        result.error = str(e)
    finally:
        if not args.keep and (run_dir / "db").exists():
            shutil.rmtree(run_dir / "db")
    return result


def format_table(results: list[Result]) -> str:
    def micros(result: Result, label: str, key: str) -> str:
        value = result.latency_micros.get(label, {}).get(key)
        return "-" if value is None else f"{value:.1f}"

    def mib(value: int | None) -> str:
        return "-" if value is None else f"{value / 1048576:.0f}"

    header = (
        "configuration",
        "rewrite s",
        "replay s",
        "wall s",
        "get p50/p95/p99 us",
        "multiget p50/p99 us",
        "write p50/p99 us",
        "db MiB",
        "sst MiB",
    )
    rows = [header]
    failures = []
    for r in results:
        if not r.ok:
            failures.append(f"{r.configuration}: FAILED: {r.error}")
            continue
        rows.append((
            r.configuration,
            f"{r.rewrite_seconds:.2f}" if r.rewrite_seconds is not None else "-",
            f"{r.replay_seconds:.2f}" if r.replay_seconds is not None else "-",
            f"{r.wall_seconds:.2f}" if r.wall_seconds is not None else "-",
            "/".join(micros(r, "get", k) for k in ("p50", "p95", "p99")),
            "/".join(micros(r, "multiget", k) for k in ("p50", "p99")),
            "/".join(micros(r, "write", k) for k in ("p50", "p99")),
            mib(r.db_bytes),
            mib(r.sst_bytes),
        ))
    widths = [max(len(row[i]) for row in rows) for i in range(len(header))]
    lines = ["  ".join(cell.ljust(width) for cell, width in zip(row, widths, strict=True)).rstrip() for row in rows]
    return "\n".join(lines + failures)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("trace_dir", type=Path, help="trace directory written by hived (holds trace_info.json)")
    parser.add_argument("configurations", type=Path, nargs="+", help="configuration files, or directories of *.ini")
    parser.add_argument("--db-bench", default="hive_db_bench", help="db_bench binary (default: hive_db_bench in PATH)")
    parser.add_argument(
        "--work-dir",
        type=Path,
        help="where the database copies go (default: <trace_dir>/replays; keep it on the checkpoint's file "
        "system so SST files are hard linked rather than copied)",
    )
    parser.add_argument("--json", type=Path, help="also write the results to this JSON file")
    parser.add_argument("--keep", action="store_true", help="keep each configuration's database copy")
    parser.add_argument(
        "--no-rewrite",
        action="store_true",
        help="replay on the checkpoint's files as hived wrote them, without compacting them under the "
        "configuration first (table options then apply only to the files the replay writes)",
    )
    parser.add_argument(
        "--drop-caches",
        action="store_true",
        help="drop the page cache before each replay (needs root), so the configurations start equally cold",
    )
    parser.add_argument(
        "--db-bench-arg", action="append", default=[], help="extra db_bench argument for every run (repeatable)"
    )
    return parser.parse_args(argv)


def expand_configurations(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        files.extend(sorted(path.glob("*.ini")) if path.is_dir() else [path])
    names = [p.stem for p in files]
    duplicates = sorted({n for n in names if names.count(n) > 1})
    if duplicates:
        raise HarnessError(f"configuration names must be unique, repeated: {', '.join(duplicates)}")
    return files


def load_trace(trace_dir: Path) -> Trace:
    info_file = trace_dir / "trace_info.json"
    if not info_file.is_file():
        raise HarnessError(f"{info_file} not found: not a trace directory written by hived")
    try:
        info = json.loads(info_file.read_text())
        checkpoint = trace_dir / info["checkpoint"]
        trace_file = trace_dir / info["trace"]
    except (json.JSONDecodeError, KeyError) as e:
        raise HarnessError(f"{info_file} is not a trace description: {e}") from e
    options = load_options_file(latest_options_file(checkpoint))
    column_families = sum(1 for s in options if s.kind == "CFOptions")
    return Trace(info, checkpoint, trace_file, options, column_families)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        trace = load_trace(args.trace_dir)
        configurations = [load_configuration(p) for p in expand_configurations(args.configurations)]
    except (HarnessError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    info = trace.info
    if not info.get("ended"):
        print(
            f"warning: the trace of {info.get('database')} did not end cleanly (hived stopped while tracing?), "
            "it replays only up to where it was cut off",
            file=sys.stderr,
        )
    work_dir = args.work_dir or args.trace_dir / "replays"
    print(
        f"{info.get('database')}: trace of blocks {info.get('start_block')}-{info.get('end_block', '?')}"
        f" (RocksDB {info.get('rocksdb_version', '?')}), {trace.column_families} column families,"
        f" {len(configurations)} configurations",
        flush=True,
    )
    results = []
    for configuration in configurations:
        print(f"replaying under {configuration.name} ...", flush=True)
        results.append(run_configuration(args, configuration, trace, work_dir))

    print(format_table(results))
    if args.json:
        args.json.write_text(json.dumps({"trace": info, "results": [asdict(r) for r in results]}, indent=2) + "\n")
    return 0 if all(r.ok for r in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
