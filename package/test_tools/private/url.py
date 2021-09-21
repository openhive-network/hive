from urllib.parse import urlparse


class Url:
    def __init__(self, url: str, *, protocol=''):
        parsed_url = urlparse(url, scheme=protocol)

        if not parsed_url.netloc:
            parsed_url = urlparse(f'//{url}', scheme=protocol)

        self.protocol = parsed_url.scheme
        self.address = parsed_url.hostname
        self.port = parsed_url.port

    def as_string(self, *, with_protocol=True) -> str:
        protocol_prefix = f'{self.protocol}://' if with_protocol else ''
        return f'{protocol_prefix}{self.address}:{self.port}'
