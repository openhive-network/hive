from __future__ import annotations

from typing import TYPE_CHECKING

from clive_local_tools.beekeeper.network import raw_http_call
from schemas.jsonrpc import JSONRPCRequest

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo


async def test_empty_sign_digest(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_empty_sign_digest will check if response from sign_digest is empty."""
    # ARRANGE
    token = beekeeper.token

    sign_digest = JSONRPCRequest(
        method="beekeeper_api.sign_digest",
        params={
            "token": token,
            "WRONG_ARG": "WRONG ARG",
            "public_key": wallet.public_key.value,
        },
    )

    # ACT
    response = await raw_http_call(http_endpoint=beekeeper.http_endpoint, data=sign_digest)

    # On current hive develop(a54419d4f6632f0d2a9c89ea690155c766cbbf81 ) is
    # {'jsonrpc': '2.0', 'error': {'code': -32003, 'message': "Assert Exception:sig_digest.size(): `sig_digest` can't be empty", 'data': {'code': 10, 'name': 'assert_exception', 'message': 'Assert Exception', 'stack': [{'context': {'level': 'error', 'file': 'beekeeper_wallet_manager.cpp', 'line': 106, 'method': 'sign_digest', 'hostname': '', 'thread_name': 'th_l', 'timestamp': '2024-02-19T10:34:31'}, 'format': "`sig_digest` can't be empty", 'data': {}}], 'extension': {'assertion_expression': 'sig_digest.size()'}}}, 'id': 1} # noqa: ERA001
    assert "`sig_digest` can't be empty" in response["error"]["message"], "Param `sig_digest` must be set."
