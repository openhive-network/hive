from __future__ import annotations

from typing import TYPE_CHECKING, Protocol

if TYPE_CHECKING:
    from beem import Hive


class NodeClientMaker(Protocol):
    def __call__(self, accounts: list[dict] | None = None) -> Hive:
        pass
