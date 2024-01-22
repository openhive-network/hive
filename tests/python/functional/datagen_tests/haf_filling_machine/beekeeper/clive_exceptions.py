from typing import TypeAlias, Any

JsonT = dict[str, Any]


class CliveError(Exception):
    """Base class for all clive exceptions."""


class CommunicationError(CliveError):
    """Base class for all communication exceptions."""


class FormValidationError(CliveError):
    def __init__(self, reason: str, *, given_value: str | None = None) -> None:
        self.reason = reason
        self.given_value = given_value
        super().__init__()


class NodeAddressError(FormValidationError):
    """Base class for all node address exceptions."""


CommunicationResponseT: TypeAlias = str | JsonT | list[JsonT]
