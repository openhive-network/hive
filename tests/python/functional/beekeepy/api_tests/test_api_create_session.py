from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError
from clive_local_tools.beekeeper import checkers
from clive_local_tools.beekeeper.constants import MAX_BEEKEEPER_SESSION_AMOUNT

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper


async def create_session(beekeeper: Beekeeper, salt: str) -> None:
    # ARRANGE
    notification_endpoint = beekeeper.notification_server_http_endpoint.as_string(with_protocol=False)
    message_to_check = f'"id":0,"jsonrpc":"2.0","method":"beekeeper_api.create_session","params":{{"notifications_endpoint":"{notification_endpoint}","salt":"{salt}"}}'

    # ACT
    token = (
        await beekeeper.api.create_session(
            notifications_endpoint=notification_endpoint,
            salt=salt,
        )
    ).token

    # ASSERT
    assert len(token), "Returned token should not be empty"
    assert checkers.check_for_pattern_in_file(
        beekeeper.get_wallet_dir() / "stderr.log", message_to_check
    ), "Log should have information about create_session call."


async def test_api_create_session(beekeeper: Beekeeper) -> None:
    """Test test_api_create_session will test beekeeper_api.create_session api call."""
    salt = "test_api_create_session"
    # ARRANGE & ACT & ASSERT
    await create_session(beekeeper, salt)


async def test_api_create_session_max_sessions(beekeeper: Beekeeper) -> None:
    """Test test_api_create_session will test max count of sessions."""
    salt = "test_api_create_session_max_sessions"

    # ARRANGE & ACT
    for _ in range(MAX_BEEKEEPER_SESSION_AMOUNT - 1):
        await create_session(beekeeper, salt)

    # ASSERT
    with pytest.raises(
        CommunicationError, match=f"Number of concurrent sessions reached a limit ==`{MAX_BEEKEEPER_SESSION_AMOUNT}`"
    ):
        await create_session(beekeeper, salt)
