from __future__ import annotations

from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

from pydantic import StrRegexError

from beekeepy.exceptions.base import BeekeepyError
from helpy._interfaces.context import ContextSync
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from types import TracebackType


class DetectableError(ContextSync[None], BeekeepyError, ABC):
    """
    Base class for conditionally raised exception using `with` statement based detection.

    Example:
    ```
    with DividsionByZeroException(a, b):
        print(a / b)
    ```

    Raises:
        self: If conditions specified by child classes in `_is_exception_handled` are met
    """

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
    """Raises when wallet with given name was not found by beekeeper."""

    def __init__(self, wallet_name: str) -> None:
        """Constructor.

        Args:
            wallet_name (str): name of wallet that was missing
        """
        self.wallet_name = wallet_name
        super().__init__(f"no such wallet with name: {wallet_name}")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and "Assert Exception:wallet->load_wallet_file(): Unable to open file: " in ex.error
            and f"{self.wallet_name}.wallet" in ex.error
        )


class WalletWithSuchNameAlreadyExistsError(DetectableError):
    """Raises during creation, when given wallet name is already taken."""

    def __init__(self, wallet_name: str) -> None:
        """Constructor.

        Args:
            wallet_name (str): name of wallet that is already taken
        """
        self.wallet_name = wallet_name
        super().__init__(f"wallet with such name already exists: {wallet_name}")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and f"Assert Exception:!bfs::exists(wallet_filename): Wallet with name: '{self.wallet_name}' already exists"
            in ex.error
        )


class InvalidPrivateKeyError(DetectableError):
    """Raises when given private key was rejected by beekeeper because of format."""

    def __init__(self, wif: str) -> None:
        """Constructor.

        Args:
            wif (str): private key that was rejected because of format
        """
        super().__init__(f"given private key is invalid: `{wif}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and "Assert Exception:false: Key can't be constructed" in ex.error


class RemovingNotExistingKeyError(DetectableError):
    """Raises when user tries to remove key that not exists."""

    def __init__(self, public_key: str) -> None:
        """Constructor.

        Args:
            public_key (str): key that user tries to remove, but it already does not exist
        """
        super().__init__(f"cannot remove key that does not exist: `{public_key}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and "Assert Exception:false: Key not in wallet" in ex.error


class MissingSTMPrefixError(DetectableError):
    """Raises when given by user public key misses STM prefix."""

    def __init__(self, public_key: str) -> None:
        """Constructor.

        Args:
            public_key (str): string that is missing STM prefix to be recognized as public key
        """
        self.public_key = public_key
        super().__init__(f"missing STM prefix in given public key: `{self.public_key}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and f"Assert Exception:source.substr( 0, prefix.size() ) == prefix: public key requires STM prefix, but was given `{self.public_key}`"
            in ex.error
        )


class InvalidPublicKeyError(DetectableError):
    """Raises when given public key was rejected by beekeeper because of format."""

    def __init__(self, public_key: str) -> None:
        """Constructor.

        Args:
            public_key (str): public key that was rejected because of format
        """
        super().__init__(f"given public key is invalid: `{public_key}`")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, RequestError) and "Assert Exception:s == sizeof(data):" in ex.error


class InvalidWalletError(DetectableError):
    """Raises when user passes invalid wallet name, which cannot be represented as file."""

    def __init__(self, wallet_name: str) -> None:
        """Constructor.

        Args:
            wallet_name (str): invalid wallet name
        """
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


class InvalidPasswordError(DetectableError):
    """Raises when user provides invalid password to unlock wallet."""

    def __init__(self, wallet_name: str) -> None:
        """Constructor.

        Args:
            wallet_name (str): name of wallet to which invalid password was passed
        """
        self.wallet_name = wallet_name
        super().__init__(f"given password is invalid for {self.wallet_name}")

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return (
            isinstance(ex, RequestError)
            and "Assert Exception:false: Invalid password for wallet:" in ex.error
            and f"{self.wallet_name}.walletrethrow" in ex.error
        )


class SchemaDetectableError(DetectableError, ABC):
    """Base class for errors that bases on schema exceptions."""

    def __init__(self, arg_name: str, arg_value: str) -> None:
        """Constructor.

        Args:
            arg_name (str): name of argument which was invalid
            arg_value (str): value of argument which was invalid
        """
        self._arg_name = arg_name
        self._arg_value = arg_value
        super().__init__(self._error_message())

    def _is_exception_handled(self, ex: BaseException) -> bool:
        return isinstance(ex, StrRegexError)

    @abstractmethod
    def _error_message(self) -> str: ...


class InvalidAccountNameError(SchemaDetectableError):
    """Raises when given user name does not match regex."""

    def _error_message(self) -> str:
        return f"Value given for argument `{self._arg_name}` is invalid account name: `{self._arg_value}`."


class InvalidSchemaPublicKeyError(SchemaDetectableError):
    """Raises when given public key does not match regex."""

    def _error_message(self) -> str:
        return f"Value given for argument `{self._arg_name}` is invalid public key: `{self._arg_value}`."


class InvalidSchemaPrivateKeyError(SchemaDetectableError):
    """Raises when given private key does not match regex."""

    def _error_message(self) -> str:
        return f"Value given for argument `{self._arg_name}` is invalid private key: `{self._arg_value}`."
