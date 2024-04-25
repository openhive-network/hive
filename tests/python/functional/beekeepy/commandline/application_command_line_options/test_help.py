from __future__ import annotations

from pathlib import Path

from clive.__private.core.beekeeper import Beekeeper


def check_help_output(generated_output: str) -> None:
    """Function will parse log file in order to search pattern."""
    pattern_file_path = Path(__file__).parent / "patterns/help_pattern.txt"
    with pattern_file_path.open() as pattern_file:
        pattern_file_text = pattern_file.read().strip()
        assert pattern_file_text == generated_output.strip(), "Generated help message is different than pattern."


def test_help() -> None:
    """Test will check command line flag --help."""
    # ARRANGE & ACT
    help_output = Beekeeper().help()

    # ASSERT
    check_help_output(generated_output=help_output)
