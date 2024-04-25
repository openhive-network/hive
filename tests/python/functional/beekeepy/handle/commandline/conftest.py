from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy._executable.beekeeper_executable import BeekeeperExecutable

if TYPE_CHECKING:
    from hive_local_tools.beekeeper.models import SettingsLoggerFactory


@pytest.fixture()
def beekeeper_exe(request: pytest.FixtureRequest, settings_with_logger: SettingsLoggerFactory) -> BeekeeperExecutable:
    incoming_settings, logger = settings_with_logger()
    return BeekeeperExecutable(settings=incoming_settings, logger=logger)
