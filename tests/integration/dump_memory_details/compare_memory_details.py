#!/usr/bin/env python3
"""
Compares replay_benchmark.jsonl, dumped by hived run with --dump-memory-details, with a pattern.

Only the values that are the same in every replay of the same blocks are compared:
- the sequence of records (block number and summary type, which is absent in regular measurements),
- the number of objects in every index.
Times, process memory, free space in shared memory and memory used by indexes are skipped, since they differ
between runs (e.g. memory held by dynamic containers depends on their capacity).

Use --store to (re)create the pattern from a dump when a change in the number of objects is expected.
"""

from __future__ import annotations

import argparse
import difflib
import json
import sys
from pathlib import Path
from typing import Any

NO_INDEX = "<no index>"


def reduce_record(record: dict[str, Any]) -> dict[str, Any]:
    reduced = {"summary": record["summary"]} if "summary" in record else {}
    reduced["block_number"] = record["block_number"]
    reduced["index_size"] = dict(
        sorted((index["index_name"], int(index["index_size"])) for index in record["index_memory_details_cntr"])
    )
    return reduced


def read_dump(path: Path) -> list[dict[str, Any]]:
    with path.open(encoding="utf-8") as dump_file:
        return [reduce_record(json.loads(line)) for line in dump_file if line.strip()]


def describe(record: dict[str, Any]) -> str:
    kind = f"{record['summary']} summary" if "summary" in record else "measurement"
    return f"{kind} at block {record['block_number']}"


def find_differences(pattern: list[dict[str, Any]], dump: list[dict[str, Any]]) -> list[str]:
    expected_records = [describe(record) for record in pattern]
    actual_records = [describe(record) for record in dump]
    if expected_records != actual_records:
        return [
            "Records differ:",
            *difflib.unified_diff(expected_records, actual_records, "pattern", "dump", lineterm=""),
        ]

    differences = []
    for expected, actual in zip(pattern, dump):
        expected_sizes = expected["index_size"]
        actual_sizes = actual["index_size"]
        for index_name in sorted(expected_sizes.keys() | actual_sizes.keys()):
            expected_size = expected_sizes.get(index_name, NO_INDEX)
            actual_size = actual_sizes.get(index_name, NO_INDEX)
            if expected_size != actual_size:
                differences.append(f"{describe(expected)}: {index_name}: expected {expected_size}, got {actual_size}")
    return differences


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pattern", type=Path, help="pattern file")
    parser.add_argument("dump", type=Path, help="replay_benchmark.jsonl dumped by hived")
    parser.add_argument("--store", action="store_true", help="store the dump as the pattern instead of comparing")
    args = parser.parse_args()

    dump = read_dump(args.dump)
    if not dump:
        print(f"No records in {args.dump}")
        return 1

    if args.store:
        with args.pattern.open("w", encoding="utf-8") as pattern_file:
            json.dump(dump, pattern_file, indent=2)
            pattern_file.write("\n")
        print(f"Stored {len(dump)} records from {args.dump} as pattern {args.pattern}")
        return 0

    with args.pattern.open(encoding="utf-8") as pattern_file:
        pattern = json.load(pattern_file)

    differences = find_differences(pattern, dump)
    if differences:
        print(f"{args.dump} does not match pattern {args.pattern}")
        print("\n".join(differences))
        print(f"If the change is expected, update the pattern: {sys.argv[0]} --store {args.pattern} {args.dump}")
        return 1

    print(f"{args.dump} matches pattern {args.pattern} ({len(dump)} records)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
