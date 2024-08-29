from __future__ import annotations

from abc import abstractmethod
from typing import TYPE_CHECKING

from helpy._interfaces.context import ContextSync
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from types import TracebackType


class DetectableError(ContextSync[None], Exception):
    @abstractmethod
    def _is_exception_handled(self, ex: BaseException) -> bool: ...

    def _enter(self) -> None:
        return None

    def _finally(self) -> None:
        return None

    def _handle_exception(self, ex: BaseException, tb: TracebackType | None) -> bool:
        if self._is_exception_handled(ex):
            raise self
        return super()._handle_exception(ex, tb)


class NoWalletWithSuchNameError(DetectableError):
    def __init__(self, wallet_name: str) -> None:
        self.wallet_name = wallet_name
        super().__init__(f"no such wallet with name: {wallet_name}")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and "Assert Exception:wallet->load_wallet_file(): Unable to open file: " in ex.error
            and f"{self.wallet_name}.wallet" in ex.error
        )


class WalletWithSuchNameAlreadyExistsError(DetectableError):
    def __init__(self, wallet_name: str) -> None:
        self.wallet_name = wallet_name
        super().__init__(f"wallet with such name already exists: {wallet_name}")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and f"Assert Exception:!bfs::exists(wallet_filename): Wallet with name: '{self.wallet_name}' already exists"
            in ex.error
        )


class InvalidPrivateKeyError(DetectableError):
    def __init__(self, wif: str) -> None:
        super().__init__(f"given private key is invalid: `{wif}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and "Assert Exception:false: Key can't be constructed" in ex.error


class RemovingNotExistingKeyError(DetectableError):
    def __init__(self, public_key: str) -> None:
        super().__init__(f"cannot remove key that does not exist: `{public_key}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and "Assert Exception:false: Key not in wallet" in ex.error


class MissingSTMPrefixError(DetectableError):
    def __init__(self, public_key: str) -> None:
        self.public_key = public_key
        super().__init__(f"missing STM prefix in given public key: `{self.public_key}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and f"Assert Exception:source.substr( 0, prefix.size() ) == prefix: public key requires STM prefix, but was given `{self.public_key}`"
            in ex.error
        )


class InvalidPublicKeyError(DetectableError):
    def __init__(self, public_key: str) -> None:
        super().__init__(f"given public key is invalid: `{public_key}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and "Assert Exception:s == sizeof(data):" in ex.error


class InvalidWalletError(DetectableError):
    def __init__(self, wallet_name: str) -> None:
        self.wallet_name = wallet_name
        super().__init__(
            f"given wllet name is invalid, only alphanumeric and '._-@' chars are allowed: `{self.wallet_name}`"
        )

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and any(
            error_message in ex.error
            for error_message in [
                "Name of wallet is incorrect. Is empty."
                f"Name of wallet is incorrect. Name: {self.wallet_name}. Only alphanumeric and '._-@' chars are allowed"
                f"Name of wallet is incorrect. Name: {self.wallet_name}. File creation with given name is impossible."
            ]
        )
