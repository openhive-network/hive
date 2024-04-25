from __future__ import annotations

from typing import TYPE_CHECKING

from clive_local_tools.beekeeper.network import raw_http_call
from schemas.jsonrpc import JSONRPCRequest

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo


async def test_sign_digest_response_uniques(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_sign_digest_response_uniques will check if signature returned from sign_digest call is unique."""
    # ARRANGE
    expected_signature_amount = 2
    token = beekeeper.token

    digests = [
        "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff",
        "dba93a07d0f3fa438e8147a536559efd8344cacdbfcaceb42bab980c56dddaee",
    ]

    signatures = set()
    for digest in digests:
        sign_digest = JSONRPCRequest(
            method="beekeeper_api.sign_digest",
            params={
                "token": token,
                "sig_digest": digest,
                "public_key": wallet.public_key.value,
            },
        )

        # ACT
        response = await raw_http_call(http_endpoint=beekeeper.http_endpoint, data=sign_digest)
        signatures.add(response["result"]["signature"])
    # ASSERT
    assert len(signatures) == expected_signature_amount, "Thee should be two different signatures."
