"""
Binary filters of operations - bit masks of operations taken by APIs of account history.

Bits of operations are given by their numbers (`operation_id` - positions in `hive::protocol::operation` of
libraries/protocol/include/hive/protocol/operations.hpp, generated into values of operations):

- `any_operation_binary_filter` - `operation_filter_low` / `operation_filter_high` of
  `account_history_api.get_account_history` (a 128-bit mask, the bit of an operation is its number),
- `virtual_operation_binary_filter` - `filter` of `account_history_api.enum_virtual_ops` (the bit of a virtual operation
  is its number counted from the first virtual operation, `fill_convert_request`) - the API takes its `low` part.
"""

from __future__ import annotations

import importlib
from dataclasses import dataclass
from functools import cache
from typing import TYPE_CHECKING, Final, Self

from hiveio_api._base import VirtualOperationBase

if TYPE_CHECKING:
    from collections.abc import Iterable

    from hiveio_api._base import OperationBase

__all__ = [
    "BinaryFilterResult",
    "any_operation_binary_filter",
    "virtual_operation_binary_filter",
]

_HALF_BITS: Final[int] = 64
_HALF_MASK: Final[int] = (1 << _HALF_BITS) - 1


@dataclass(frozen=True, kw_only=True)
class BinaryFilterResult:
    """
    Bit mask of operations split into two 64-bit halves, as APIs take it.

    Attributes:
        high: Bits 64-127 of the mask (e.g. `operation_filter_high`).
        low: Bits 0-63 of the mask (e.g. `operation_filter_low`, `filter` of `enum_virtual_ops`).
    """

    high: int
    low: int

    @classmethod
    def from_mask(cls, mask: int) -> Self:
        """Halves of the mask."""
        return cls(high=mask >> _HALF_BITS, low=mask & _HALF_MASK)


def any_operation_binary_filter(*op_cls: type[OperationBase]) -> BinaryFilterResult:
    """
    Filter of the given operations of `get_account_history` - the bit of an operation is its number.

    Example:
        `any_operation_binary_filter(VoteOperation, ProducerMissedOperation)` - `low` with bit 0 (`vote`), `high` with
        bit 22 (`producer_missed` is the operation 86).
    """
    return BinaryFilterResult.from_mask(_mask(operation.operation_id for operation in op_cls))


def virtual_operation_binary_filter(*vop_cls: type[VirtualOperationBase]) -> BinaryFilterResult:
    """
    Filter of the given virtual operations of `enum_virtual_ops` - bits counted from the first virtual operation.

    Example:
        `virtual_operation_binary_filter(FillConvertRequestOperation, AuthorRewardOperation)` - `low` with bits 0 and 1.

    Raises:
        ValueError: When an operation is not virtual.
    """
    if regular := [operation.get_name() for operation in vop_cls if not operation.virtual]:
        raise ValueError(f"Operations {regular} are not virtual - use `any_operation_binary_filter`")
    first = _first_virtual_operation_id()
    return BinaryFilterResult.from_mask(_mask(operation.operation_id - first for operation in vop_cls))


def _mask(bits: Iterable[int]) -> int:
    mask = 0
    for bit in bits:
        mask |= 1 << bit
    return mask


@cache
def _first_virtual_operation_id() -> int:
    """Number of the first virtual operation - virtual operations follow regular ones in `hive::protocol::operation`."""
    importlib.import_module(f"{__package__}.values")  # defines values of all operations
    return min(operation.operation_id for operation in VirtualOperationBase.__subclasses__())
