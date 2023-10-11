from __future__ import annotations

from typing import Protocol

from beem import Hive


class NodeClientMaker(Protocol):
    def __call__(self, accounts: list[dict] = None) -> Hive:
        pass
