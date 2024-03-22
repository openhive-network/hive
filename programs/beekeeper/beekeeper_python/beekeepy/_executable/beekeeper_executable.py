from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._executable.beekeeper_executable_discovery import get_beekeeper_binary_path
from beekeepy._executable.executable import Executable

if TYPE_CHECKING:
    from loguru import Logger

    from beekeepy.settings import Settings


class BeekeeperExecutable(Executable):
    def __init__(self, settings: Settings, logger: Logger) -> None:
        super().__init__(settings.binary_path or get_beekeeper_binary_path(), settings.working_directory, logger)
