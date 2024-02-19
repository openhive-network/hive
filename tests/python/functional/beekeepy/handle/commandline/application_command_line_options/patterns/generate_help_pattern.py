from __future__ import annotations

from pathlib import Path

import loguru
from beekeepy._executable.beekeeper_executable import BeekeeperExecutable
from beekeepy.settings import Settings

if __name__ == "__main__":
    help_text = BeekeeperExecutable(settings=Settings(), logger=loguru.logger).get_help_text()
    help_pattern_file = Path.cwd() / "help_pattern.txt"
    with help_pattern_file.open(mode="w") as help_pattern:
        help_pattern.write(help_text)
