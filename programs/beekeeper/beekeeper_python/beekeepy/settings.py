from __future__ import annotations

from pathlib import Path

from pydantic import Field

from helpy import HttpUrl
from helpy import Settings as HandleSettings


class Settings(HandleSettings):
    """Defines parameters for handles how to start and behave."""

    working_directory: Path = Field(default_factory=lambda: Path.cwd())
    http_endpoint: HttpUrl | None = None  # type: ignore[assignment]
    notification_endpoint: HttpUrl | None = None
    binary_path: Path | None = None
