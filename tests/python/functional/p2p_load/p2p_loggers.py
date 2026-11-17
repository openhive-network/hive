"""Which opt-in p2p loggers a node gets: the p2p statistics and relay sample loggers, only where its hived has them."""

from __future__ import annotations

import json
import mmap
import re
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path

# the periodic counter report and the sampled relay timestamps; a build without them gets neither logger
LOGGERS = ("p2p_stats", "p2p_relay_sample")
_LEVEL = "info"
_APPENDER = "p2p"
_CONFIG_LINE = re.compile(r"^\s*log-logger\s*=\s*(.*?)\s*$", re.MULTILINE)
_LOGGER_ENTRY = re.compile(r"\{[^{}]*\}")


class P2pLoggersError(Exception):
    """A node's config.ini does not enable the loggers the harness meant to enable."""


def binary_loggers(hived: Path) -> list[str]:
    """
    Return the opt-in p2p loggers the given hived binary writes to.

    hived accepts any logger name in `log-logger` and has no option naming these, so support is read from the
    binary itself: a build that writes to a logger carries its name as a C string.
    """
    with hived.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as image:
        return [name for name in LOGGERS if image.find(name.encode() + b"\0") != -1]


def with_loggers(log_logger: str | None, loggers: list[str]) -> str:
    """Return the `log-logger` value with the given loggers added at info, writing to the p2p log."""
    entries = [
        json.dumps({"name": name, "level": _LEVEL, "appender": _APPENDER}, separators=(",", ":")) for name in loggers
    ]
    return " ".join([*([log_logger] if log_logger else []), *entries])


def _enabled_in_config(config_text: str) -> set[str]:
    """Return the names of the opt-in p2p loggers a config.ini enables at info or a more verbose level."""
    enabled: set[str] = set()
    for value in _CONFIG_LINE.findall(config_text):
        for entry in _LOGGER_ENTRY.findall(value):
            logger = json.loads(entry)
            if logger.get("name") in LOGGERS and logger.get("level") in {"all", "debug", "info"}:
                enabled.add(logger["name"])
    return enabled


def verify_config(config_path: Path, loggers: list[str]) -> None:
    """Raise P2pLoggersError unless the node's config.ini enables exactly the given opt-in p2p loggers."""
    found = _enabled_in_config(config_path.read_text())
    if found != set(loggers):
        raise P2pLoggersError(f"{config_path}: enables p2p loggers {sorted(found)}, expected {sorted(loggers)}")
