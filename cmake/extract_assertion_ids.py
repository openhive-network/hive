#!/usr/bin/env python3
"""Extract FC_ASSERT expressions and emit code for assertion id generation."""
from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Iterator, TextIO


PROJECT_ROOT: pathlib.Path = pathlib.Path(__file__).resolve().parents[1]
LOG_FILE_PATH: pathlib.Path = pathlib.Path("/tmp/extract_assertion_ids.log")


def find_all_assertion_macros(project_root: pathlib.Path) -> list[str]:
    """Return assertion-like macro names declared in the specialised exceptions header."""

    file_with_macro = (
        project_root
        / "libraries"
        / "protocol"
        / "include"
        / "hive"
        / "protocol"
        / "hive_specialised_exceptions.hpp"
    )
    if not file_with_macro.is_file():
        return []

    pattern: re.Pattern[str] = re.compile(r"^#define\s+(HIVE_[A-Z0-9_]+)\s*\(.*")
    result: list[str] = []
    for line in file_with_macro.open("r", encoding="utf-8"):
        if line.startswith("#define") and ((match := pattern.match(line)) is not None):
            result.append(match.group(1))
    return result


def log(message: str, *, stream: TextIO | None = None) -> None:
    """Write *message* to the provided stream (stdout by default)."""

    print(message, file=sys.stdout if stream is None else stream)


recognized_assertions: tuple[str, ...] = (
    "FC_ASSERT",
    *find_all_assertion_macros(PROJECT_ROOT),
)

ASSERTION_NAME_PATTERN: str = ")|(?:".join(recognized_assertions)
FIND_REGEX: str = r"(?!\#define)[ ]+" + f"((?:{ASSERTION_NAME_PATTERN}))" r"\s*\("
_FC_ASSERT_PATTERN = re.compile(FIND_REGEX)

_COMMENT_SANITIZE_PATTERN = re.compile(r'\\"|//|/\*|\*/|\\\n|"')

def _iter_fc_assertions(source: str) -> Iterator[str]:
    """Yield the expression found inside each FC_ASSERT invocation."""
    position = 0
    length = len(source)
    while True:
        match = _FC_ASSERT_PATTERN.search(source, position)
        if not match:
            break
        index = match.end()
        depth = 1
        in_string = False
        escaped = False
        while index < length:
            char = source[index]
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = not in_string
            elif not in_string:
                if char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
                    if depth == 0:
                        break
            index += 1

        if depth != 0:
            # Unbalanced expression; continue scanning after the match.
            position = match.end()
            continue

        yield source[match.end():index]
        position = index + 1


def _trim_assertion_expression(expr: str) -> str:
    """Trim everything after the first comma outside parentheses or strings."""
    depth = 0
    in_string = False
    escaped = False
    for offset, char in enumerate(expr):
        if escaped:
            escaped = False
            continue
        if char == "\\":
            escaped = True
            continue
        if char == '"':
            in_string = not in_string
            continue
        if in_string:
            continue
        if char == "(":
            depth += 1
            continue
        if char == ")" and depth > 0:
            depth -= 1
            continue
        if char == "," and depth == 0:
            return expr[:offset]
    return expr


def _sanitize_for_comment(expr: str) -> str:
    """Prepare the expression for embedding inside a C++ comment."""
    sanitized = _COMMENT_SANITIZE_PATTERN.sub("**", expr)
    sanitized = re.sub(r"[\r\n]+", " ", sanitized)
    return sanitized.replace("\\0", "")


def _process_file(path: pathlib.Path, namespace: str) -> list[str]:
    """Return generated lines for a single source file."""
    contents = path.read_text(encoding="utf-8", errors="ignore")
    assertions = list(_iter_fc_assertions(contents))
    if not assertions:
        return []

    generated: list[str] = [f'  fsv << "//{path.name}" << std::endl;']
    for assertion in assertions:
        trimmed = _trim_assertion_expression(assertion).strip()
        sanitized = _sanitize_for_comment(trimmed)
        generated.append(f'  h = HASH_EXPR( {trimmed} );')
        generated.append(
            f'  fsv << "uint64_t assertion_" << h << " = " << h << "ull; /*{path.name}*/ /*{sanitized}*/ " << std::endl;'
        )
        generated.append(
            f'  fsw << "container.insert( std::make_pair( " << h << "ull, \\"{namespace}\\") );" << std::endl;'
        )
    return generated


def _iter_source_paths(list_file: pathlib.Path) -> Iterator[pathlib.Path]:
    """Yield absolute paths extracted from the provided list file."""
    with list_file.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line:
                continue
            yield pathlib.Path(line).expanduser().resolve()


def main() -> int:
    """Process source files listed in *argv* and emit assertion metadata."""

    parser = argparse.ArgumentParser(
        description="Generate assertion id helper code from FC_ASSERT invocations.",
    )
    parser.add_argument(
        "source_list",
        help="File containing newline separated list of assertion source files.",
    )
    parser.add_argument("assertion_id_namespace", help="Namespace for the generated assertion ids.")
    parser.add_argument("assertion_id_verifier_header_path", help="Path to the verifier header file.")
    parser.add_argument("assertion_id_wax_inline_path", help="Path to the wax inline file.")
    parser.add_argument("output", help="Path to write output.")

    arguments = parser.parse_args(list(sys.argv)[1:])

    assertion_id_verifier_header_path = arguments.assertion_id_verifier_header_path
    assertion_id_wax_inline_path = arguments.assertion_id_wax_inline_path
    namespace = str(arguments.assertion_id_namespace)
    output = pathlib.Path(arguments.output).expanduser().resolve()

    list_path = pathlib.Path(arguments.source_list).expanduser().resolve()
    if not list_path.is_file():
        log(f"Source list '{list_path}' does not exist.", stream=sys.stderr)
        return 1

    generated_lines: list[str] = []
    for source_path in _iter_source_paths(list_path):
        if source_path.name == "hive_specialised_exceptions.hpp":
            continue
        if not source_path.is_file():
            log(f"Source file '{source_path}' does not exist.", stream=sys.stderr)
            return 1
        generated_lines.extend(_process_file(source_path, namespace))

    output.parent.mkdir(parents=True, exist_ok=True)
    pathlib.Path(assertion_id_verifier_header_path).parent.mkdir(parents=True, exist_ok=True)
    pathlib.Path(assertion_id_wax_inline_path).parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", errors="ignore") as output_file:
        output_file.write("""
#include <fc/exception/exception.hpp>
#include <fstream>

#define HASH_EXPR( EXPR ) fc::exception::hash_expr( #EXPR );


int main( int argc, char** argv )
{
    uint64_t h = 0;
    std::fstream fsv, fsw;
    fsv.open(""" + '"' + assertion_id_verifier_header_path + '"' + """, std::ios::out | std::ios::trunc);
    fsv << "#include <cstdint>" << std::endl;
    fsw.open(""" + '"' + assertion_id_wax_inline_path + '"' + """, std::ios::out | std::ios::trunc);
""")
        for line in generated_lines:
            output_file.write("  " + line + "\n")
        output_file.write("""
    fsv.close();
    fsw.close();
    return 0;
}""")
        

    return 0


if __name__ == "__main__":
    sys.exit(main())
