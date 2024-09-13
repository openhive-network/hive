from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING, AsyncIterator, Final, Iterator

import pytest
from beekeepy._handle import AsyncBeekeeper
from beekeepy._interface.delay_guard import DelayGuardBase

from hive_local_tools.beekeeper.generators import default_wallet_credentials
from hive_local_tools.beekeeper.models import SettingsLoggerFactory, WalletInfo
from hive_local_tools.beekeeper.network import async_raw_http_call
from schemas.jsonrpc import JSONRPCRequest

if TYPE_CHECKING:
    from helpy import HttpUrl as Url


# We have 500ms time period protection on ulocking wallet.
WALLET_UNLOCK_INTERVAL: Final[float] = DelayGuardBase.BEEKEEPER_DELAY_TIME.total_seconds()
AES_DECRYPTION_ERROR: Final[str] = "Invalid password for wallet"
WALLET_UNACCESSIBLE_ERROR: Final[str] = "Assert Exception:false: unlock is not accessible"


@pytest.fixture()
def beekeeper_not_started(settings_with_logger: SettingsLoggerFactory) -> Iterator[AsyncBeekeeper]:
    incoming_settings, logger = settings_with_logger()
    bk = AsyncBeekeeper(settings=incoming_settings, logger=logger)

    yield bk

    if bk.is_running:
        bk.close()


@pytest.fixture()
async def beekeeper(beekeeper_not_started: AsyncBeekeeper) -> AsyncIterator[AsyncBeekeeper]:
    async with beekeeper_not_started as bk:
        yield bk


@pytest.fixture()
async def wallet(beekeeper: AsyncBeekeeper) -> WalletInfo:
    name, password = default_wallet_credentials()
    await beekeeper.api.create(wallet_name=name, password=password)
    return WalletInfo(name=name, password=password)


async def call_and_raise_if_error(http_endpoint: Url, data: JSONRPCRequest) -> None:
    """
    Function will query http_endpoint with given data, and raise AssertError exception if there is an error in response.

    Example:
    -------
        {'jsonrpc': '2.0', 'error': {'code': -32003, 'message': 'Assert Exception:false: unlock is not accessible', 'data': {'code': 10, 'name': 'assert_exception', 'message': 'Assert Exception', 'stack': [{'context': {'level': 'error', 'file': 'beekeeper_wallet_api.cpp', 'line': 107, 'method': 'unlock', 'hostname': '', 'thread_name': 'th_l', 'timestamp': '2024-02-19T12:24:56'}, 'format': 'unlock is not accessible', 'data': {}}], 'extension': {'assertion_expression': 'false'}}}, 'id': 1}
    """
    response = await async_raw_http_call(http_endpoint=http_endpoint, data=data)

    if "error" in response:
        raise AssertionError(response["error"]["message"])


@pytest.mark.parametrize("force_error", [True, False])
async def test_wallet_blocking_timeout(beekeeper: AsyncBeekeeper, wallet: WalletInfo, force_error: bool) -> None:
    """Test test_wallet_blocking_timeout will test wallet blocking 500ms interval."""
    # ARRANGE
    wallets = (await beekeeper.api.list_wallets()).wallets
    assert wallets[0].name == wallet.name

    unlock_jsons = []
    assert beekeeper.settings.notification_endpoint is not None
    for i in range(5):
        session = (
            await beekeeper.api.create_session(
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
        await call_and_raise_if_error(http_endpoint=beekeeper.http_endpoint, data=wrong_password_unlock_call)

    if force_error:
        tasks = [
            call_and_raise_if_error(http_endpoint=beekeeper.http_endpoint, data=unlock_json)
            for unlock_json in good_unlock_calls
        ]

        results = await asyncio.gather(*tasks, return_exceptions=True)
        errors_correctly_raised = [str(result) == WALLET_UNACCESSIBLE_ERROR for result in results]
        assert len(errors_correctly_raised) == len(tasks), "Not every call raised the expected error."
    else:
        for unlock_json in good_unlock_calls:
            await asyncio.sleep(WALLET_UNLOCK_INTERVAL)
            await call_and_raise_if_error(http_endpoint=beekeeper.http_endpoint, data=unlock_json)
