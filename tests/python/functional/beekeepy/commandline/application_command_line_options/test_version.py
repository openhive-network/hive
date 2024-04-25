from __future__ import annotations

import json

from clive.__private.core.beekeeper import Beekeeper


def check_version_output(version_output: str) -> None:
    data = json.loads(version_output)
    assert "version" in data
    assert data["version"] is not None
    assert isinstance(data["version"], str)


def test_version() -> None:
    """Test will check command line flag --version."""
    # ARRANGE & ACT
    version_output = Beekeeper().version()

    # ASSERT
    check_version_output(version_output)
