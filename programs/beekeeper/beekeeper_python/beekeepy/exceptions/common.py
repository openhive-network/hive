from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy.exceptions.base import BeekeepyError

if TYPE_CHECKING:
    from helpy import HttpUrl


class BeekeeperExecutableError(BeekeepyError):
    """Base class for exceptions related to starting/closing beekeeper executable."""


class BeekeeperIsNotRunningError(BeekeeperExecutableError):
    """Raises if after user tries to access options avilable only when process is running."""


class BeekeeperAlreadyRunningError(BeekeepyError):
    """Raises if beekeeper executable is already running, by capturing this exception you can get connection information to already running instance."""

    def __init__(self, address: HttpUrl, pid: int) -> None:  # noqa: D107
        self.address = address
        self.pid = pid
        super().__init__(f"Beekeeper is already running under {address=} with {pid=}")


class WalletIsLockedError(BeekeepyError):
    """Raises if marked function requires wallet to be unlocked."""

    def __init__(self, wallet_name: str) -> None:  # noqa: D107
        self.wallet_name = wallet_name
        super().__init__(f"Wallet `{wallet_name}` is locked.")


class TimeoutReachWhileCloseError(BeekeepyError):
    """Raises when beekeeper did not closed during specified timeout."""

    def __init__(self) -> None:
        """Constructor."""
        super().__init__("Process was force-closed with SIGKILL, because didn't close before timeout")


class UnknownDecisionPathError(BeekeepyError):
    """Error created to suppress mypy error: `Missing return statement  [return]`."""


class NotPositiveTimeError(BeekeepyError):
    """Raises when given time value was 0 or negative."""

    def __init__(self, time: int) -> None:
        """Constructor.

        Args:
            time (int): invalid time value
        """
        super().__init__(f"Given time value is not positive: `{time}`.")


class InvalidWalletNameError(BeekeepyError):
    """Raises when specified wallet name was not matching alphanumeric and extra charachters conditions."""

    def __init__(self, wallet_name: str) -> None:
        """Constructor.

        Args:
            wallet_name (str): invalid wallet name
        """
        super().__init__(
            f"Given wallet name is invalid: `{wallet_name}`. Can be only alphanumeric or contain `._-@` charachters."
        )
