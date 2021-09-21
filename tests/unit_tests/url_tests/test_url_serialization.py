from test_tools.private.url import Url


def test_serialization():
    assert Url('127.0.0.1:1024', protocol='http').as_string() == 'http://127.0.0.1:1024'
    assert Url('127.0.0.1:1024', protocol='http').as_string(with_protocol=False) == '127.0.0.1:1024'

    assert Url('127.0.0.1:1024').as_string(with_protocol=False) == '127.0.0.1:1024'
