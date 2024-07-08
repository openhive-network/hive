from __future__ import annotations

from typing import Protocol


class SyncWalletLocked(Protocol):
    def __call__(self, wallet_names: list[str]) -> None: ...


class AsyncWalletLocked(Protocol):
    async def __call__(self, wallet_names: list[str]) -> None: ...
