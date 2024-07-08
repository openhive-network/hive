from __future__ import annotations

from datetime import timedelta
from pathlib import Path

from pydantic import Field

from helpy import HttpUrl
from helpy import Settings as HandleSettings


class Settings(HandleSettings):
    """Defines parameters for beekeeper how to start and behave."""

    working_directory: Path = Field(default_factory=lambda: Path.cwd())
    """Path, where beekeeper binary will store all it's data and logs."""

    http_endpoint: HttpUrl | None = None  # type: ignore[assignment]
    """Endpoint on which python will comunicate with beekeeper, required for remote beekeeper."""

    notification_endpoint: HttpUrl | None = None
    """Endpoint to use for reverse communication between beekeeper and python."""

    binary_path: Path | None = None
    """Alternative path to beekeeper binary."""

    propagate_sigint: bool = True
    """If set to True (default), sigint will be sent to beekeeper without controll of this library."""

    close_timeout: timedelta = Field(default_factory=lambda: timedelta(seconds=10.0))
    """This timeout varriable affects time handle waits before beekeepy closes."""
