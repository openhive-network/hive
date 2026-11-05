"""URLs of hiveio-api-client encoded and decoded by `hiveio_api.validation` (hived's dumped config holds them)."""

from __future__ import annotations

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

URLS = pytest.mark.parametrize(
    ("url_type", "address"),
    [("HttpUrl", "http://127.0.0.1:8090"), ("WsUrl", "ws://127.0.0.1:8090"), ("P2PUrl", "0.0.0.0:2001")],
)


@URLS
def test_url_of_hiveio_api_client_is_encoded_by_its_serialize(url_type: str, address: str) -> None:
    # ARRANGE
    import hiveio_api_client
    from hiveio_api.validation.encoders import enc_hook_hf26, get_hf26_encoder

    url = getattr(hiveio_api_client, url_type)(address)

    # ACT & ASSERT
    assert enc_hook_hf26(url) == url.serialize()
    assert get_hf26_encoder().encode({"endpoint": url}) == b'{"endpoint":"' + url.serialize().encode() + b'"}'


@URLS
def test_url_of_hiveio_api_client_is_decoded_from_its_serialized_form(url_type: str, address: str) -> None:
    # ARRANGE
    import hiveio_api_client
    import msgspec
    from hiveio_api.validation.decoders import dec_hook_hf26, get_hf26_decoder

    url_class = getattr(hiveio_api_client, url_type)
    url = url_class(address)
    config = msgspec.defstruct("Config", [("endpoint", url_class)])

    # ACT
    decoded = get_hf26_decoder(config).decode(b'{"endpoint":"' + url.serialize().encode() + b'"}')

    # ASSERT
    assert dec_hook_hf26(url_class, url.serialize()) == url
    assert isinstance(decoded.endpoint, url_class)
    assert decoded.endpoint == url
