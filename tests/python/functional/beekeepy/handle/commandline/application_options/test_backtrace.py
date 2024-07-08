from __future__ import annotations

import time
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
            beekeeper_exe.working_directory.as_posix(),
            f"--backtrace={backtrace}",
        ],
    ):
        time.sleep(0.1)
        # ASSERT
        assert checkers.check_for_pattern_in_file(
            beekeeper_exe.working_directory / "stderr.log", "Backtrace on segfault is enabled."
        ) is (backtrace == "yes")
