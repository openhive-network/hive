import pytest

from test_tools.private.url import Url


@pytest.mark.parametrize(
    'input_url, expected_protocol, expected_address, expected_port',
    [
        ('http://127.0.0.1:8080', 'http', '127.0.0.1', 8080),
        ('ws://127.0.0.1:8080', 'ws', '127.0.0.1', 8080),
    ]
)
def test_url_parsing_without_expected_protocol(input_url, expected_protocol, expected_address, expected_port):
    url = Url(input_url)

    assert url.protocol == expected_protocol
    assert url.address == expected_address
    assert url.port == expected_port


@pytest.mark.parametrize(
    'input_url, expected_protocol, expected_address, expected_port',
    [
        ('127.0.0.1:8080', 'http', '127.0.0.1', 8080),
        ('127.0.0.1:8080', 'ws', '127.0.0.1', 8080),
    ]
)
def test_url_parsing_with_expected_protocol(input_url, expected_protocol, expected_address, expected_port):
    url = Url(input_url, protocol=expected_protocol)

    assert url.protocol == expected_protocol
    assert url.address == expected_address
    assert url.port == expected_port
