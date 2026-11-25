#!/usr/bin/env python3
"""Write a junit report for a suite whose cases are steps of a script.

    junit_cases.py JUNIT SUITE CASES
    junit_cases.py --cause LOG

CASES is a file of tab-separated lines the script appended as it went:

    case<TAB>NAME<TAB>pass|fail|skip<TAB>SECONDS<TAB>MESSAGE[<TAB>LOG]

A failing case's body is its cause, read from LOG when one is given: each of
ninja's FAILED: blocks (the output of the failed command, then the command
itself, shortened), then the log's tail. The blocks come first, so a reader that
keeps only the start of the text still sees what failed; a log with no FAILED:
line (a test step) gives the tail alone. The first FAILED: line is also appended
to the failure message. `--cause` prints the body for LOG.
"""

from __future__ import annotations

import os
import re
import sys
from xml.sax.saxutils import escape, quoteattr

LOG_TAIL_LINES = 80
# The FAILED: blocks kept, and the lines kept of each: a broken header fails
# every unit including it, and the first few name it.
FAILED_BLOCKS = 5
FAILED_BLOCK_LINES = 40
COMMAND_CHARS = 300
CONTROL = re.compile(r"\x1b\[[0-9;]*[A-Za-z]|[\x00-\x08\x0b\x0c\x0e-\x1f]")
# What ends the output ninja prints for a failed command: another command's
# progress line or ninja's own verdict.
NINJA_LINE = re.compile(r"^(\[\d+/\d+\]|ninja: )")


def log_lines(path: str) -> list[str]:
    if not path or not os.path.isfile(path):
        return []
    with open(path, encoding="utf-8", errors="replace") as handle:
        return CONTROL.sub("", handle.read()).splitlines()


def failed_blocks(lines: list[str]) -> list[list[str]]:
    """Ninja prints `FAILED: <outputs>`, the command, then the command's output."""
    blocks = []
    for i, line in enumerate(lines):
        if not line.startswith("FAILED: "):
            continue
        command = lines[i + 1] if i + 1 < len(lines) and not NINJA_LINE.match(lines[i + 1]) else ""
        output = []
        for following in lines[i + 2 if command else i + 1 :]:
            if NINJA_LINE.match(following) or following.startswith("FAILED: "):
                break
            output.append(following)
        output = output[:FAILED_BLOCK_LINES]
        if len(command) > COMMAND_CHARS:
            command = command[:COMMAND_CHARS] + "..."
        blocks.append([line.rstrip(), *output] + ([f"command: {command}"] if command else []))
        if len(blocks) == FAILED_BLOCKS:
            break
    return blocks


def failure_body(lines: list[str]) -> str:
    tail = "\n".join(lines[-LOG_TAIL_LINES:])
    blocks = failed_blocks(lines)
    if not blocks:
        return tail
    return "\n\n".join("\n".join(block) for block in blocks) + f"\n\n--- last {LOG_TAIL_LINES} lines ---\n" + tail


def failure_message(message: str, lines: list[str]) -> str:
    first = next((line.rstrip() for line in lines if line.startswith("FAILED: ")), "")
    return f"{message}; {first}" if first else message


def main(junit: str, suite: str, cases_file: str) -> None:
    cases = []
    with open(cases_file, encoding="utf-8") as handle:
        for line in handle.read().splitlines():
            fields = line.split("\t")
            if fields[0] != "case" or len(fields) < 4:
                continue
            fields += [""] * (6 - len(fields))
            _, name, status, seconds, message, log = fields[:6]
            cases.append((name, status, float(seconds or 0), message, log))
    body = []
    for name, status, seconds, message, log in cases:
        head = f"<testcase classname={quoteattr(suite)} name={quoteattr(name)} time=\"{seconds}\">"
        if status == "fail":
            lines = log_lines(log)
            body.append(
                f"{head}<failure message={quoteattr(failure_message(message, lines))}>"
                f"{escape(failure_body(lines))}</failure></testcase>"
            )
        elif status == "skip":
            body.append(f"{head}<skipped message={quoteattr(message)}/></testcase>")
        else:
            body.append(f"{head}</testcase>")
    failures = sum(1 for case in cases if case[1] == "fail")
    skipped = sum(1 for case in cases if case[1] == "skip")
    total = sum(case[2] for case in cases)
    os.makedirs(os.path.dirname(junit) or ".", exist_ok=True)
    with open(junit, "w", encoding="utf-8") as handle:
        handle.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        handle.write(
            f"<testsuite name={quoteattr(suite)} tests=\"{len(cases)}\" failures=\"{failures}\" "
            f"errors=\"0\" skipped=\"{skipped}\" time=\"{total}\">\n"
        )
        handle.write("\n".join(body) + "\n</testsuite>\n")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--cause":
        print(failure_body(log_lines(sys.argv[2])))
    elif len(sys.argv) == 4:
        main(*sys.argv[1:])
    else:
        sys.exit(f"usage: {sys.argv[0]} JUNIT SUITE CASES | --cause LOG")
