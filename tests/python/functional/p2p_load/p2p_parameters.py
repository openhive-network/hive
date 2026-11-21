"""Which `p2p-parameters` keys a node gets: only the ones given, and only those its hived binary knows."""

from __future__ import annotations

import json
import re
import subprocess
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from pathlib import Path

# anchored on the option, not its description, whose sentences change between builds
_HELP_DEFAULT = re.compile(r"--p2p-parameters arg\s.*?\(Default:\s+(\{.*?\})\s+\)", re.DOTALL)
_CONFIG_LINE = re.compile(r"^\s*p2p-parameters\s*=\s*(.*?)\s*$", re.MULTILINE)


class P2pParametersError(Exception):
    """A binary's help or a node's config.ini does not carry what the harness expects."""


def defaults_from_help(help_text: str) -> dict[str, Any]:
    """
    Return the default `p2p-parameters` object that `hived --help` prints.

    Its keys are exactly the parameters the binary accepts. Raises P2pParametersError when the help has no
    `--p2p-parameters` entry.
    """
    match = _HELP_DEFAULT.search(help_text)
    if match is None:
        raise P2pParametersError("hived --help shows no --p2p-parameters default")
    # boost wraps the description into indented lines; the JSON has no spaces, so the breaks are the only whitespace
    defaults: dict[str, Any] = json.loads(re.sub(r"\s*\n\s*", "", match.group(1)))
    return defaults


def binary_defaults(hived: Path) -> dict[str, Any]:
    """Return the default `p2p-parameters` of the given hived binary."""
    result = subprocess.run([str(hived), "--help"], capture_output=True, text=True, check=False, timeout=120)
    try:
        return defaults_from_help(result.stdout + result.stderr)
    except P2pParametersError as error:
        raise P2pParametersError(f"{hived}: {error} (exit {result.returncode})") from error


@dataclass(frozen=True)
class Selection:
    applied: dict[str, Any]
    unsupported: list[str]


def select(given: dict[str, Any], defaults: dict[str, Any]) -> Selection:
    """
    Split the given parameters into those the binary knows (applied) and those it lacks (left out).

    A parameter that is not given is never applied, so the node runs the binary's default for it.
    """
    return Selection(
        applied={key: value for key, value in given.items() if key in defaults},
        unsupported=sorted(key for key in given if key not in defaults),
    )


def from_config(config_text: str) -> dict[str, Any] | None:
    """Return the `p2p-parameters` object a config.ini sets, or None when it sets none."""
    values = _CONFIG_LINE.findall(config_text)
    if not values:
        return None
    if len(values) > 1:
        raise P2pParametersError(f"config.ini sets p2p-parameters {len(values)} times")
    parameters: dict[str, Any] = json.loads(values[0])
    return parameters


def verify_config(config_path: Path, applied: dict[str, Any]) -> None:
    """Raise P2pParametersError unless the node's config.ini sets exactly the applied parameters (or none)."""
    found = from_config(config_path.read_text())
    expected = applied or None
    if found != expected:
        raise P2pParametersError(f"{config_path}: p2p-parameters is {found}, expected {expected}")
