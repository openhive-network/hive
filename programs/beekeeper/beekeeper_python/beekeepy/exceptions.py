from __future__ import annotations

from typing import TYPE_CHECKING

from helpy.exceptions import HelpyError

if TYPE_CHECKING:
    from helpy import HttpUrl


class BeekeepyError(HelpyError):
    """Base class for all exception raised by beekeepy."""


class BeekeeperExecutableError(BeekeepyError):
    """Base class for exceptions related to starting/closing beekeeper executable."""


class BeekeeperIsNotRunningError(BeekeeperExecutableError):
    """Raised if after user tries to access options avilable only when process is running."""


class BeekeeperAlreadyRunningError(BeekeepyError):
    """Raised if beekeeper executable is already running, by capturing this exception you can get connection information to already running instance."""

    def __init__(self, address: HttpUrl, pid: int) -> None:  # noqa: D107
        self.address = address
        self.pid = pid
        super().__init__(f"Beekeeper is already running under {address=} with {pid=}")


class WalletIsLockedError(BeekeepyError):
    """Raised if marked function requires wallet to be unlocked."""

    def __init__(self, wallet_name: str) -> None:  # noqa: D107
        self.wallet_name = wallet_name
        super().__init__(f"Wallet `{wallet_name}` is locked.")
