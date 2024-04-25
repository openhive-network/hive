from __future__ import annotations

from typing import TYPE_CHECKING, Final

import pytest
from helpy.exceptions import CommunicationError, RequestError

from hive_local_tools.beekeeper import checkers

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

WRONG_TOKEN: Final[str] = "104fc637d5c32c271bdfdc366af5bfc8f977e2462b01877454cfd1643196bcf1"


def test_api_close_session(beekeeper: Beekeeper) -> None:
    """Test test_api_close_session will test beekeeper_api.close_session api call."""
    # ARRANGE & ACT
    beekeeper.api.close_session()

    # ASSERT
    close_log_entry = f'"id":0,"jsonrpc":"2.0","method":"beekeeper_api.close_session","params":{{"token":"{beekeeper.session_token}"}}'
    with pytest.raises(
        RequestError,
        match=f"A session attached to {beekeeper.session_token} doesn't exist",
    ):
        beekeeper.api.close_session()

    assert checkers.check_for_pattern_in_file(
        beekeeper.settings.working_directory / "stderr.log", close_log_entry
    ), "Log should have information about closing session with specific token created during create_session call."


def test_if_beekeeper_closes_after_last_session_termination(
    beekeeper: Beekeeper,
) -> None:
    """Test test_api_close_session will test if beekeeper closes after closing last session."""
    # ARRANGE & ACT
    beekeeper.api.close_session()
    beekeeper.close()

    # ASSERT
    with pytest.raises(CommunicationError, match="no response available"):
        beekeeper.api.list_wallets()

    assert checkers.check_for_pattern_in_file(
        beekeeper.settings.working_directory / "stderr.log", "exited cleanly"
    ), "Beekeeper should be closed after last session termination."


def test_x_api_close_session_double(beekeeper: Beekeeper) -> None:
    """Test test_api_close_session will test possibility of double closing session."""
    # ARRANGE & ACT
    beekeeper.api.close_session()

    # ASSERT
    with pytest.raises(
        RequestError,
        match=f"A session attached to {beekeeper.session_token} doesn't exist",
    ):
        beekeeper.api.close_session()


@pytest.mark.parametrize("create_session", [False, True], ids=["no_session_before", "in_other_session"])
def test_api_close_session_not_existing(create_session: bool, beekeeper: Beekeeper) -> None:
    """Test test_api_close_session_not_existing will test possibility of closing not existing session."""
    # ARRANGE
    if create_session:
        assert beekeeper.settings.notification_endpoint is not None
        beekeeper.api.create_session(
            notifications_endpoint=beekeeper.settings.notification_endpoint.as_string(with_protocol=False),
            salt="salt",
        )

    # ACT & ASSERT
    beekeeper._set_session_token(WRONG_TOKEN)  # noqa: SLF001
    with pytest.raises(RequestError, match=f"A session attached to {WRONG_TOKEN} doesn't exist"):
        beekeeper.api.close_session()
