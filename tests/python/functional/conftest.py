from __future__ import annotations

import logging

from test_tools.__private.scope.scope_fixtures import *  # noqa: F403


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger("urllib3.connectionpool").propagate = False
