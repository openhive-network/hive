from __future__ import annotations

from typing import Any, Generic, Literal, Self, TypeVar, cast, get_args, overload
from urllib.parse import parse_qs, urlencode, urlparse

from schemas.fields.resolvables import Resolvable

P2PProtocolT = Literal[""]
HttpProtocolT = Literal["http", "https"]
WsProtocolT = Literal["ws", "wss"]
ProtocolT = TypeVar("ProtocolT", bound=HttpProtocolT | WsProtocolT | P2PProtocolT)


class Url(Resolvable["Url[ProtocolT]", str], Generic[ProtocolT]):
    """Wrapper for Url, for handy access to all of it members with serialization."""

    __protocol: str
    __address: str
    __port: int | None
    __path: str
    __query: dict[str, Any]

    @overload
    def __init__(self, /, url_or_address: str | Url[ProtocolT], *, protocol: ProtocolT | None = None) -> None: ...

    @overload
    def __init__(
        self,
        /,
        url_or_address: str,
        *,
        port: int | None = None,
        path: str = "",
        query: dict[str, Any] | str = "",
        protocol: ProtocolT | None = None,
    ) -> None: ...

    def __init__(
        self,
        /,
        url_or_address: str | Url[ProtocolT],
        *,
        port: int | None = None,
        path: str = "",
        query: dict[str, Any] | str = "",
        protocol: ProtocolT | None = None,
    ) -> None:
        self.__validate_proto(protocol)

        if isinstance(url_or_address, Url):
            self.__copy_from_existing_url(url_or_address)
        elif any([port, path, query]) and isinstance(url_or_address, str):
            self.__fill_url(
                address=url_or_address,
                port=port,
                protocol=protocol,
                path=path,
                query=query,
            )
        elif isinstance(url_or_address, str):
            self.__parse_url_string(url_or_address, protocol)
        else:
            raise TypeError("Unknown type, cannot convert to Url")

    def __str__(self) -> str:
        return self.as_string()

    def __repr__(self) -> str:
        return self.__str__()

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Url):
            return self.as_string() == other.as_string()
        return super().__eq__(other)

    def __hash__(self) -> int:
        return hash(self.as_string())

    @property
    def protocol(self) -> ProtocolT:
        """Return protocol of url, e.x: http, https."""
        return self.__protocol  # type: ignore[return-value]

    @property
    def address(self) -> str:
        """Return address or hostname of url, e.x: 127.0.0.1, localhost."""
        return self.__address

    @property
    def port(self) -> int | None:
        """Return port of url, e.x: 0, 8090."""
        return self.__port

    @property
    def path(self) -> str:
        """Return path of url, e.x: api/v1."""
        return self.__path

    @property
    def query(self) -> dict[str, Any]:
        """Return query of url, e.x: {'key': 'value', 'key2': ['value2', 'value3']}."""
        return self.__query

    def as_string(self, *, with_protocol: bool = True) -> str:
        """Serializes url."""
        protocol_prefix = f"{self.protocol}://" if with_protocol and self.__protocol else ""
        port_suffix = f":{self.port}" if self.port is not None else ""
        path_suffix = f"/{self.__path}" if self.__path else ""
        query_suffix = f"?{urlencode(self.__query, doseq=True, encoding='utf-8')}" if self.__query else ""

        return f"{protocol_prefix}{self.address}{port_suffix}{path_suffix}{query_suffix}"

    @classmethod
    def _allowed_protocols(cls) -> list[str]:
        """Returns allowed protocols.

        Note: at index 0 should be default protocol.
        """
        return [""]

    @classmethod
    def factory(
        cls,
        *,
        port: int | None = None,
        address: str = "127.0.0.1",
        path: str = "",
        query: dict[str, Any] | str = "",
        protocol: ProtocolT | None = None,
    ) -> Self:
        return cls(url_or_address=address, port=port, path=path, query=query, protocol=protocol)

    @classmethod
    def _default_protocol(cls) -> str:
        return cls._allowed_protocols()[0]

    @staticmethod
    def resolve(incoming_cls: type, value: str) -> Url[ProtocolT]:
        """Resolve string to Url."""
        return cast("Url[ProtocolT]", incoming_cls(value))

    def serialize(self) -> Any:
        return self.as_string(with_protocol=False)

    @classmethod
    def __decode_query(cls, query: str) -> dict[str, Any]:
        """Decode query string to dictionary."""
        if not query.strip("?"):
            return {}

        parsed_query_string = parse_qs(query, encoding="utf-8", strict_parsing=True)
        result: dict[str, Any] = {}
        for key, value in parsed_query_string.items():
            if (not key) or (not value):
                raise ValueError(f"Query key and value cannot be empty. {key=} | {value=}")

            result[key] = value[0] if len(value) == 1 else value
        return cls.__dearray_query_value(result)

    @classmethod
    def __dearray_query_value(cls, query: dict[str, Any]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in query.items():
            if isinstance(value, list | tuple) and len(value) == 1:
                result[key] = value[0]
            else:
                result[key] = value
        return result

    def __copy_from_existing_url(self, url: Url[ProtocolT]) -> None:
        """Copy values from existing Url."""
        self.__protocol = url.__protocol
        self.__address = url.__address
        self.__port = url.__port
        self.__path = url.__path
        self.__query = url.__query

    def __parse_url_string(self, url: str, protocol: ProtocolT | None = None) -> None:
        """Parse url string and set values."""
        parsed_url = urlparse(url, scheme=protocol or self._default_protocol())
        if not parsed_url.netloc:
            parsed_url = urlparse(f"//{url}", scheme=protocol or self._default_protocol())

        self.__validate_proto(parsed_url.scheme)

        if not parsed_url.hostname:
            raise ValueError("Address was not specified.")

        self.__fill_url(
            address=parsed_url.hostname,
            port=parsed_url.port,
            protocol=cast("ProtocolT", parsed_url.scheme),
            path=parsed_url.path,
            query=parsed_url.query,
        )

    def __fill_url(
        self,
        address: str,
        port: int | None,
        protocol: ProtocolT | None,
        path: str,
        query: dict[str, Any] | str,
    ) -> None:
        self.__address = address
        self.__protocol = protocol or self._default_protocol()
        self.__port = port
        self.__path = path.strip("/")
        if isinstance(query, str):
            self.__query = self.__decode_query(query)
        else:
            self.__query = self.__dearray_query_value(query)

    @classmethod
    def __validate_proto(cls, protocol: str | None) -> None:
        """Validate protocol."""
        allowed_proto = cls._allowed_protocols()
        if protocol is not None and protocol not in allowed_proto:
            raise ValueError(f"Unknown protocol: `{protocol}`, allowed: {allowed_proto}")


class HttpUrl(Url[HttpProtocolT]):
    @classmethod
    def _allowed_protocols(cls) -> list[str]:
        return list(get_args(HttpProtocolT))


class WsUrl(Url[WsProtocolT]):
    @classmethod
    def _allowed_protocols(cls) -> list[str]:
        return list(get_args(WsProtocolT))


class P2PUrl(Url[P2PProtocolT]):
    pass


AnyUrl = HttpUrl | WsUrl | P2PUrl
