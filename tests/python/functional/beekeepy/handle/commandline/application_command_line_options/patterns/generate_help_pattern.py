from __future__ import annotations

from pathlib import Path

import loguru
from beekeepy import Settings
from beekeepy._executable.beekeeper_executable import BeekeeperExecutable

if __name__ == "__main__":
    help_text = BeekeeperExecutable(settings=Settings(), logger=loguru.logger).get_help_text()
    help_pattern_file = Path.cwd() / "help_pattern.txt"
    help_pattern_file.write_text(help_text.strip())
