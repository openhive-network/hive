from __future__ import annotations

from pathlib import Path

from clive.__private.core.beekeeper import Beekeeper

if __name__ == "__main__":
    help_text = Beekeeper().help()
    help_pattern_file = Path.cwd() / "help_pattern.txt"
    with help_pattern_file.open(mode="w") as help_pattern:
        help_pattern.write(help_text)
