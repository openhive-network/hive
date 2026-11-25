"""
Hived options a run sets per node (`--hived-option`), written into the node's config.ini: only options its hived
reads from a config file, a `log-logger` entry in place of the one of that logger's name.
"""

from __future__ import annotations

import json
import re
import subprocess
import tempfile
from pathlib import Path
from typing import Any

from python.functional.p2p_load import p2p_loggers

# hived writes a default config.ini, one commented `# key = default` line per config file option, when it has none
_DEFAULT_CONFIG_OPTION = re.compile(r"^#?\s*([a-z0-9][a-z0-9-]*) =(?: |$)", re.MULTILINE)
_CONFIG_LINE = re.compile(r"^\s*([a-z0-9][a-z0-9-]*)\s*=\s*(.*?)\s*$", re.MULTILINE)


class HivedOptionsError(Exception):
    """A hived has no such config option, or a node's config.ini does not carry an option the run sets."""


def options_from_default_config(config_text: str) -> set[str]:
    """Return the option names of the default config.ini hived writes."""
    return set(_DEFAULT_CONFIG_OPTION.findall(config_text))


def config_options(hived: Path) -> set[str]:
    """
    Return the options the given hived reads from its config file.

    hived ignores a config.ini line it has no option for, so a misspelt or too new option would pass unnoticed; its
    `--help` also lists command-line-only options. The default config.ini it writes lists exactly the former.
    """
    with tempfile.TemporaryDirectory() as data_dir:
        result = subprocess.run(
            [str(hived), "--data-dir", data_dir, "--dump-config"],
            capture_output=True,
            text=True,
            check=False,
            timeout=120,
        )
        config = Path(data_dir) / "config.ini"
        if not config.is_file():
            raise HivedOptionsError(f"{hived} --dump-config wrote no config.ini (exit {result.returncode})")
        return options_from_default_config(config.read_text())


def check_known(hived: Path, options: list[tuple[str, str]], known: set[str]) -> None:
    """Raise HivedOptionsError naming the options `hived` does not read from its config file."""
    unknown = sorted({key for key, _ in options} - known)
    if unknown:
        raise HivedOptionsError(f"{hived} has no config option {', '.join(unknown)}")


def with_logger(log_logger: str | None, logger: str) -> str:
    """Return the `log-logger` value with the given logger object in place of the one of its name, or added."""
    entry = json.loads(logger)
    entries = [old for old in p2p_loggers.logger_entries(log_logger) if old.get("name") != entry["name"]]
    return " ".join(json.dumps(e, separators=(",", ":")) for e in [*entries, entry])


def apply(config: Any, options: list[tuple[str, str]]) -> None:
    """
    Set `options` on a node's test-tools config.

    A `log-logger` value replaces that logger's entry. An option the config has a field for is set there (added, to
    a list such as `plugin`). The config has no field for most hived options and writes the value of `log_logger`
    as it is, so the others ride on that line, one more line each.
    """
    extra: list[str] = []
    for key, value in options:
        member = key.replace("-", "_")
        if key == "log-logger":
            config.log_logger = with_logger(config.log_logger, value)
        elif hasattr(config, member):
            current = getattr(config, member)
            if isinstance(current, list):
                current.append(value)
            else:
                setattr(config, member, value)
        else:
            extra.append(f"{key} = {value}")
    if extra:
        config.log_logger = "\n".join([config.log_logger or "", *extra])


def verify_config(config_path: Path, options: list[tuple[str, str]]) -> None:
    """Raise HivedOptionsError unless the node's config.ini carries every one of `options`."""
    lines: dict[str, list[str]] = {}
    for key, value in _CONFIG_LINE.findall(config_path.read_text()):
        lines.setdefault(key, []).append(value.strip('"'))
    loggers = [entry for value in lines.get("log-logger", []) for entry in p2p_loggers.logger_entries(value)]
    for key, value in options:
        found = json.loads(value) in loggers if key == "log-logger" else value in lines.get(key, [])
        if not found:
            raise HivedOptionsError(f"{config_path}: no {key} = {value} (has {lines.get(key, [])})")
