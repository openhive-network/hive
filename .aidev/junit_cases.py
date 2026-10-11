#!/usr/bin/env python3
"""Write a junit report for a suite whose cases are steps of a script.

    junit_cases.py JUNIT SUITE CASES

CASES is a file of tab-separated lines the script appended as it went:

    case<TAB>NAME<TAB>pass|fail|skip<TAB>SECONDS<TAB>MESSAGE[<TAB>LOG]

A failing case's body is the tail of LOG when one is given.
"""
import os
import re
import sys
from xml.sax.saxutils import escape, quoteattr

LOG_TAIL_LINES = 80
CONTROL = re.compile(r"\x1b\[[0-9;]*[A-Za-z]|[\x00-\x08\x0b\x0c\x0e-\x1f]")


def log_tail(path: str) -> str:
    if not path or not os.path.isfile(path):
        return ""
    with open(path, encoding="utf-8", errors="replace") as handle:
        lines = CONTROL.sub("", handle.read()).splitlines()
    return "\n".join(lines[-LOG_TAIL_LINES:])


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
            body.append(f"{head}<failure message={quoteattr(message)}>{escape(log_tail(log))}</failure></testcase>")
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
    if len(sys.argv) != 4:
        sys.exit(f"usage: {sys.argv[0]} JUNIT SUITE CASES")
    main(*sys.argv[1:])
