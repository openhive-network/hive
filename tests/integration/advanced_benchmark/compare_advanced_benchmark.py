#!/usr/bin/env python3
"""
Compares advanced_benchmark.json, dumped by hived run with --advanced-benchmark, with a pattern.

Only the values that are the same in every replay of the same blocks are compared:
- the block number the data was dumped at,
- the set of measured items (name and context),
- the number of measurements of every item (count).
Times are skipped, since they differ between runs. For reactions to flush notifications only their presence is
compared, because during replay the state is flushed at randomly chosen blocks (see
database::schedule_next_flush_block), so the number of flushes differs between runs.

Use --store to (re)create the pattern from a dump when a change in the number of measurements is expected.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

RANDOM_COUNT_ITEMS = {"flush"}  # names of items with count that differs between runs
NOT_MEASURED = "<not measured>"
MEASURED = "<measured>"  # count of RANDOM_COUNT_ITEMS


def reduce_dump(dump: dict[str, Any]) -> dict[str, Any]:
    counts: dict[str, dict[str, int | None]] = {}
    for item in dump["items"]:
        count = None if item["op_name"] in RANDOM_COUNT_ITEMS else int(item["count"])
        counts.setdefault(item["op_name"], {})[item["context"]] = count
    return {"block_number": dump["block_number"], "counts": counts}


def describe(name: str, context: str) -> str:
    return f"{name} ({context or 'evaluator'})"


def show(count: int | None | str) -> str:
    return MEASURED if count is None else str(count)


def find_differences(pattern: dict[str, Any], dump: dict[str, Any]) -> list[str]:
    differences = []
    if pattern["block_number"] != dump["block_number"]:
        differences.append(f"block number: expected {pattern['block_number']}, got {dump['block_number']}")

    for name in sorted(pattern["counts"].keys() | dump["counts"].keys()):
        expected_counts = pattern["counts"].get(name, {})
        actual_counts = dump["counts"].get(name, {})
        for context in sorted(expected_counts.keys() | actual_counts.keys()):
            expected = expected_counts.get(context, NOT_MEASURED)
            actual = actual_counts.get(context, NOT_MEASURED)
            if expected != actual:
                differences.append(f"{describe(name, context)}: expected {show(expected)}, got {show(actual)}")
    return differences


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pattern", type=Path, help="pattern file")
    parser.add_argument("dump", type=Path, help="advanced_benchmark.json dumped by hived")
    parser.add_argument("--store", action="store_true", help="store the dump as the pattern instead of comparing")
    args = parser.parse_args()

    with args.dump.open(encoding="utf-8") as dump_file:
        dump = reduce_dump(json.load(dump_file))
    items = sum(len(counts) for counts in dump["counts"].values())
    if items == 0:
        print(f"No items in {args.dump}")
        return 1

    if args.store:
        with args.pattern.open("w", encoding="utf-8") as pattern_file:
            json.dump(dump, pattern_file, indent=2, sort_keys=True)
            pattern_file.write("\n")
        print(f"Stored {items} items from {args.dump} as pattern {args.pattern}")
        return 0

    with args.pattern.open(encoding="utf-8") as pattern_file:
        pattern = json.load(pattern_file)

    differences = find_differences(pattern, dump)
    if differences:
        print(f"{args.dump} does not match pattern {args.pattern}")
        print("\n".join(differences))
        print(f"If the change is expected, update the pattern: {sys.argv[0]} --store {args.pattern} {args.dump}")
        return 1

    print(f"{args.dump} matches pattern {args.pattern} ({items} items)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
