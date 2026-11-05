"""URLs of hiveio-api-client encoded by `hiveio_api.validation.encoders` (hived's dumped config holds them)."""

from __future__ import annotations

import pytest

from tests.generated_package import require_generated_package

require_generated_package()


@pytest.mark.parametrize(
    ("url_type", "address"),
    [("HttpUrl", "http://127.0.0.1:8090"), ("WsUrl", "ws://127.0.0.1:8090"), ("P2PUrl", "0.0.0.0:2001")],
)
def test_url_of_hiveio_api_client_is_encoded_by_its_serialize(url_type: str, address: str) -> None:
    # ARRANGE
    import hiveio_api_client
    from hiveio_api.validation.encoders import enc_hook_hf26, get_hf26_encoder

    url = getattr(hiveio_api_client, url_type)(address)

    # ACT & ASSERT
    assert enc_hook_hf26(url) == url.serialize()
    assert get_hf26_encoder().encode({"endpoint": url}) == b'{"endpoint":"' + url.serialize().encode() + b'"}'
