from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Protocol

if TYPE_CHECKING:
    from beekeepy.settings import Settings
    from loguru import Logger

    import test_tools as tt
    from schemas.fields.basic import PublicKey


@dataclass
class WalletInfo:
    name: str
    password: str


@dataclass
class WalletInfoWithImportedAccounts(WalletInfo):
    accounts: list[tt.Account]

    def get_all_public_keys(self, *, with_prefix: bool = False) -> list[PublicKey]:
        return [acc.public_key.use_prefix(use=with_prefix) for acc in self.accounts]


class WalletsGeneratorT(Protocol):
    def __call__(
        self, count: int, *, import_keys: bool = True, keys_per_wallet: int = 1
    ) -> list[WalletInfoWithImportedAccounts]: ...


class SettingsFactory(Protocol):
    def __call__(self, settings_update: Settings | None = None) -> Settings: ...


class SettingsLoggerFactory(Protocol):
    def __call__(self, settings_update: Settings | None = None) -> tuple[Settings, Logger]: ...
