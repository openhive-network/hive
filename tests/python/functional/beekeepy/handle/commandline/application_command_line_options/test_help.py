from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy._executable.beekeeper_executable import BeekeeperExecutable


def check_help_output(generated_output: str) -> None:
    """Function will parse log file in order to search pattern."""
    pattern_file_path = Path(__file__).parent / "patterns/help_pattern.txt"
    pattern_file_content = pattern_file_path.read_text().strip()
    generated_output = generated_output.strip()
    assert pattern_file_content == generated_output, "Generated help message is different than pattern."


def test_help(beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check command line flag --help."""
    # ARRANGE & ACT
    help_output = beekeeper_exe.get_help_text()

    # ASSERT
    check_help_output(generated_output=help_output)
