from typing import List, Protocol

from beem import Hive


class NodeClientMaker(Protocol):
    def __call__(self, accounts: List[dict] = None) -> Hive:
        pass
