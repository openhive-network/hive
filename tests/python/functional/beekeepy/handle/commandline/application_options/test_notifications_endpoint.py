from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy._executable.arguments.beekeeper_arguments import BeekeeperArguments
from helpy import HttpUrl

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import Beekeeper


@pytest.mark.parametrize(
    "notifications_endpoint",
    [HttpUrl("0.0.0.0:0", protocol="http"), HttpUrl("127.0.0.1:0", protocol="http")],
)
def test_notifications_endpoint(beekeeper_not_started: Beekeeper, notifications_endpoint: HttpUrl) -> None:
    """
    Test will check command line flag --notifications-endpoint.

    In this test we will re-use built-in notification server. We will not pass a port here,
    because built-in notification server will get free port and use it.

    In order to test flag notifications-endpoint we will pass this flag, and internal
    we will pass port that already has been taken by notification http server to beekeeper
    executable.
    """
    # 1 pass notification flag
    # 2 inside start function there is special if, that will check if we have explicitly pass
    #   notification-endpoint flag, and append to it already taken port. This way we will
    #   point beekeeper where to send notifications.

    # ARRANGE & ACT & ASSERT
    beekeeper_not_started.run(
        additional_cli_arguments=BeekeeperArguments(notifications_endpoint=notifications_endpoint)
    )
