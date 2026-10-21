"""
Hand-written methods of generated public models - behaviour kept from the hand-written models of schemas.

The generator assigns them in the bodies of the models (`api_generation/model_methods.py`), e.g.
`__eq__ = status_item_eq` in `AppStatusStatusItem`.
"""

from __future__ import annotations

from types import NotImplementedType
from typing import TYPE_CHECKING, ClassVar, Final, Protocol

if TYPE_CHECKING:
    from collections.abc import Mapping

__all__ = ["status_item_eq"]

_NOT_IMPLEMENTED: Final[NotImplementedType] = NotImplemented
"""`NotImplemented` typed - type checkers treat it specially only in methods named like comparisons."""


class _StatusItem(Protocol):
    """A model of an application status - a dataclass with the name of the status."""

    __dataclass_fields__: ClassVar[Mapping[str, object]]
    status: str


def status_item_eq(self: _StatusItem, other: object) -> bool | NotImplementedType:
    """
    Compare with another status item or with the name of a status.

    A status item equals its name, so a status is checked like `"chain API ready" in app_status.statuses`.
    Like other models, a status item is not hashable.
    """
    if isinstance(other, str):
        return self.status == other
    if type(other) is not type(self):
        return _NOT_IMPLEMENTED
    return all(getattr(self, name) == getattr(other, name) for name in self.__dataclass_fields__)
