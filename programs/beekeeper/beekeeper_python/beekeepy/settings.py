from __future__ import annotations

from pathlib import Path  # noqa: TCH003

from pydantic import Field

from beekeepy._executable.beekeeper_executable_discovery import get_beekeeper_binary_path
from helpy import Settings as HandleSettings


class Settings(HandleSettings):
    """Defines parameters for handles how to start and behave."""

    binary_path: Path = Field(default_factory=get_beekeeper_binary_path)
