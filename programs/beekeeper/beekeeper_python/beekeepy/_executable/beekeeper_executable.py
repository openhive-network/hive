from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._executable.beekeeper_executable_discovery import get_beekeeper_binary_path
from beekeepy._executable.executable import Executable

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger


class BeekeeperExecutable(Executable):
    def __init__(self, working_directory: Path, logger: Logger) -> None:
        super().__init__(get_beekeeper_binary_path(), working_directory, logger)
