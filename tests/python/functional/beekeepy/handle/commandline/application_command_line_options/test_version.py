from __future__ import annotations

import json
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy._executable.beekeeper_executable import BeekeeperExecutable


def check_version_output(version_output: str) -> None:
    data = json.loads(version_output)
    assert "version" in data
    assert data["version"] is not None
    assert isinstance(data["version"], str)


def test_version(beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check command line flag --version."""
    # ASSERT, ARRANGE & ACT
    check_version_output(beekeeper_exe.version())
