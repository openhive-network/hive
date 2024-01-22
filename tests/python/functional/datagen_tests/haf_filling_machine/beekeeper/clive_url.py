from __future__ import annotations

from dataclasses import dataclass
from urllib.parse import urlparse

from .clive_exceptions import NodeAddressError


@dataclass(frozen=True)
class Url:
    proto: str
    host: str
    port: int | None = None

    @classmethod
    def parse(cls, url: str, *, protocol: str = "") -> Url:
        parsed_url = urlparse(url, scheme=protocol)

        if not parsed_url.netloc:
            parsed_url = urlparse(f"//{url}", scheme=protocol)

        if not parsed_url.hostname:
            raise NodeAddressError("Address was not specified.")

        return Url(parsed_url.scheme, parsed_url.hostname, parsed_url.port)

    def as_string(self, *, with_protocol: bool = True) -> str:
        protocol_prefix = f"{self.proto}://" if with_protocol else ""
        port_suffix = f":{self.port}" if self.port is not None else ""

        return f"{protocol_prefix}{self.host}{port_suffix}"

    def __str__(self) -> str:
        return self.as_string()
