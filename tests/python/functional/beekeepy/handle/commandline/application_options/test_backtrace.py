from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools.beekeeper import checkers

if TYPE_CHECKING:
    from beekeepy._executable.beekeeper_executable import BeekeeperExecutable


@pytest.mark.parametrize("backtrace", ["yes", "no"])
def test_backtrace(backtrace: str, beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check command line flag --backtrace."""
    # ARRAGNE & ACT

    with beekeeper_exe.run(
        blocking=False,
        arguments=[
            "-d",
            beekeeper_exe.woring_dir.as_posix(),
            f"--backtrace={backtrace}",
        ],
    ):
        # ASSERT
        assert checkers.check_for_pattern_in_file(
            beekeeper_exe.woring_dir / "stderr.log", "Backtrace on segfault is enabled."
        ) is (backtrace == "yes")
