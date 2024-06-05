from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import RequestError

from hive_local_tools.beekeeper import checkers
from hive_local_tools.beekeeper.constants import MAX_BEEKEEPER_SESSION_AMOUNT

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper


def create_session(beekeeper: Beekeeper, salt: str) -> None:
    # ARRANGE
    assert beekeeper.settings.notification_endpoint is not None
    notification_endpoint = beekeeper.settings.notification_endpoint.as_string(with_protocol=False)
    message_to_check = f'"id":0,"jsonrpc":"2.0","method":"beekeeper_api.create_session","params":{{"notifications_endpoint":"{notification_endpoint}","salt":"{salt}"}}'

    # ACT
    token = (
        beekeeper.api.create_session(
            notifications_endpoint=notification_endpoint,
            salt=salt,
        )
    ).token

    # ASSERT
    assert len(token), "Returned token should not be empty"
    assert checkers.check_for_pattern_in_file(
        beekeeper.settings.working_directory / "stderr.log", message_to_check
    ), "Log should have information about create_session call."


def test_api_create_session(beekeeper: Beekeeper) -> None:
    """Test test_api_create_session will test beekeeper_api.create_session api call."""
    salt = "test_api_create_session"
    # ARRANGE & ACT & ASSERT
    create_session(beekeeper, salt)


def test_api_create_session_max_sessions(beekeeper: Beekeeper) -> None:
    """Test test_api_create_session will test max count of sessions."""
    salt = "test_api_create_session_max_sessions"

    # ARRANGE & ACT
    for _ in range(MAX_BEEKEEPER_SESSION_AMOUNT):
        create_session(beekeeper, salt)

    # ASSERT
    with pytest.raises(
        RequestError, match=f"Number of concurrent sessions reached a limit ==`{MAX_BEEKEEPER_SESSION_AMOUNT}`"
    ):
        create_session(beekeeper, salt)
