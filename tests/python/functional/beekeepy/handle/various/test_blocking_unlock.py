from __future__ import annotations

import time
from typing import TYPE_CHECKING, Final

import pytest

from hive_local_tools.beekeeper.network import raw_http_call
from schemas.jsonrpc import JSONRPCRequest

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper
    from helpy import HttpUrl as Url

    from hive_local_tools.beekeeper.models import WalletInfo


# We have 500ms time period protection on ulocking wallet.
WALLET_UNLOCK_INTERVAL: Final[float] = 0.6
AES_DECRYPTION_ERROR: Final[str] = "Invalid password for wallet"
WALLET_UNACCESSIBLE_ERROR: Final[str] = "Assert Exception:false: unlock is not accessible"


def call_and_raise_if_error(http_endpoint: Url, data: JSONRPCRequest) -> str:
    """
    Function will query http_endpoint with given data, and raise AssertError exception if there is an error in response.

    Example:
    -------
        {'jsonrpc': '2.0', 'error': {'code': -32003, 'message': 'Assert Exception:false: unlock is not accessible', 'data': {'code': 10, 'name': 'assert_exception', 'message': 'Assert Exception', 'stack': [{'context': {'level': 'error', 'file': 'beekeeper_wallet_api.cpp', 'line': 107, 'method': 'unlock', 'hostname': '', 'thread_name': 'th_l', 'timestamp': '2024-02-19T12:24:56'}, 'format': 'unlock is not accessible', 'data': {}}], 'extension': {'assertion_expression': 'false'}}}, 'id': 1}
    """
    response = raw_http_call(http_endpoint=http_endpoint, data=data)

    if "error" in response:
        return response["error"]["message"]
    return ""


def raise_if_exception(exception: str) -> None:
    if exception:
        raise AssertionError(exception)


@pytest.mark.parametrize("force_error", [True, False])
def test_wallet_blocking_timeout(beekeeper: Beekeeper, wallet: WalletInfo, force_error: bool) -> None:
    """Test test_wallet_blocking_timeout will test wallet blocking 500ms interval."""
    # ARRANGE
    wallets = (beekeeper.api.list_wallets()).wallets
    assert wallets[0].name == wallet.name

    unlock_jsons = []
    assert beekeeper.settings.notification_endpoint is not None
    for i in range(5):
        session = (
            beekeeper.api.create_session(
                notifications_endpoint=beekeeper.settings.notification_endpoint.as_string(with_protocol=False),
                salt=f"salt-{i}",
            )
        ).token
        unlock_json = JSONRPCRequest(
            method="beekeeper_api.unlock",
            params={
                "token": session,
                "wallet_name": wallet.name,
                "password": wallet.password if i > 0 else "WRONG_PASSWORD",
            },
        )
        unlock_jsons.append(unlock_json)

    wrong_password_unlock_call = unlock_jsons[0]
    good_unlock_calls = unlock_jsons[1:]

    # ACT & ASSERT
    with pytest.raises(AssertionError, match=AES_DECRYPTION_ERROR):
        raise_if_exception(
            call_and_raise_if_error(http_endpoint=beekeeper.http_endpoint, data=wrong_password_unlock_call)
        )

    if force_error:
        results = [
            call_and_raise_if_error(http_endpoint=beekeeper.http_endpoint, data=unlock_json)
            for unlock_json in good_unlock_calls
        ]

        errors_correctly_raised = [str(result) == WALLET_UNACCESSIBLE_ERROR for result in results]
        assert len(errors_correctly_raised) == len(results), "Not every call raised the expected error."
    else:
        for unlock_json in good_unlock_calls:
            time.sleep(WALLET_UNLOCK_INTERVAL)
            raise_if_exception(call_and_raise_if_error(http_endpoint=beekeeper.http_endpoint, data=unlock_json))
