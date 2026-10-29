from __future__ import annotations

from ._fields import Identifier, Version

__all__ = [
    "BlockId",
    "HardforkVersion",
    "Revision",
    "TransactionId",
]

HardforkVersion = Version
TransactionId = Identifier
BlockId = Identifier
Revision = Identifier
