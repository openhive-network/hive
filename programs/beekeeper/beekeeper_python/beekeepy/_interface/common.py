from __future__ import annotations

import asyncio
from abc import ABC, abstractmethod
from asyncio import iscoroutinefunction
from functools import wraps
from typing import TYPE_CHECKING, Any, Generic, NoReturn, ParamSpec, TypeVar, overload

from beekeepy._handle.beekeeper import AsyncRemoteBeekeeper, SyncRemoteBeekeeper
from beekeepy._handle.callbacks_protocol import AsyncWalletLocked, SyncWalletLocked
from beekeepy._interface.delay_guard import AsyncDelayGuard, SyncDelayGuard
from beekeepy._interface.state_invalidator import StateInvalidator
from beekeepy.exceptions import WalletIsLockedError

if TYPE_CHECKING:
    from collections.abc import Awaitable, Callable

    from schemas.apis.beekeeper_api.fundaments_of_responses import WalletDetails

P = ParamSpec("P")
ResultT = TypeVar("ResultT")
BeekeeperT = TypeVar("BeekeeperT", bound=SyncRemoteBeekeeper | AsyncRemoteBeekeeper)
CallbackT = TypeVar("CallbackT", bound=AsyncWalletLocked | SyncWalletLocked)
GuardT = TypeVar("GuardT", bound=SyncDelayGuard | AsyncDelayGuard)


class ContainsWalletName(ABC):
    @property
    @abstractmethod
    def name(self) -> str:
        ...


class WalletCommons(ContainsWalletName, StateInvalidator, Generic[BeekeeperT, CallbackT, GuardT]):
    def __init__(
        self, *args: Any, name: str, beekeeper: BeekeeperT, session_token: str, guard: GuardT, **kwargs: Any
    ) -> None:
        super().__init__(*args, **kwargs)
        self.__name = name
        self.__beekeeper = beekeeper
        self.__last_check_is_locked = True
        self.__wallet_close_callbacks: list[CallbackT] = []
        self.session_token = session_token
        self._guard = guard

    def register_wallet_close_callback(self, callback: CallbackT) -> None:
        self._wallet_close_callbacks.append(callback)

    @property
    def _wallet_close_callbacks(self) -> list[CallbackT]:
        return self.__wallet_close_callbacks

    @property
    def name(self) -> str:
        return self.__name

    @property
    def _beekeeper(self) -> BeekeeperT:
        return self.__beekeeper

    @property
    def _last_lock_state(self) -> bool:
        """Returns last save lock state.

        True: last state of wallet was locked
        False: last state of wallet was unlocked
        """
        return self.__last_check_is_locked

    @_last_lock_state.setter
    def _last_lock_state(self, value: bool) -> None:
        self.__last_check_is_locked = value

    def _is_wallet_locked(self, *, wallet_name: str, wallets: list[WalletDetails]) -> bool:
        """Checks is wallet locked.

        Args:
            wallet_name: wallet to check is locked
            wallets: collection of wallets from api

        Returns:
            True if given wallet is locked (or not found), False if wallet is unlocked.
        """
        for wallet in wallets:
            if wallet.name == wallet_name:
                return not wallet.unlocked
        return True

    def _raise_wallet_is_locked_error(self, wallet_name: str) -> NoReturn:
        raise WalletIsLockedError(wallet_name=wallet_name)

    async def _async_call_callback_if_locked(self, *, wallet_name: str, token: str) -> None:
        assert isinstance(self._beekeeper, AsyncRemoteBeekeeper), "invalid beekeeper type, require asynchronous"
        wallets = (await self._beekeeper.api.list_wallets(token=token)).wallets
        if self._is_wallet_locked(wallet_name=wallet_name, wallets=wallets):
            if self._last_lock_state is False:
                wallet_names = [w.name for w in wallets if w.unlocked is False]
                await asyncio.gather(
                    *[
                        callback(wallet_names)
                        for callback in self._wallet_close_callbacks
                        if asyncio.iscoroutinefunction(callback)
                    ]
                )
            self._raise_wallet_is_locked_error(wallet_name=wallet_name)

    def _sync_call_callback_if_locked(self, *, wallet_name: str, token: str) -> None:
        assert isinstance(self._beekeeper, SyncRemoteBeekeeper), "invalid beekeeper type, require synchronous"
        wallets = self._beekeeper.api.list_wallets(token=token).wallets
        if self._is_wallet_locked(wallet_name=wallet_name, wallets=wallets):
            if self._last_lock_state is False:
                wallet_names = [w.name for w in wallets if w.unlocked is False]
                for callback in self.__wallet_close_callbacks:
                    callback(wallet_names)
            self._raise_wallet_is_locked_error(wallet_name=wallet_name)

    @overload
    @classmethod
    def check_wallet(cls, wrapped_function: Callable[P, ResultT]) -> Callable[P, ResultT]: ...

    @overload
    @classmethod
    def check_wallet(cls, wrapped_function: Callable[P, Awaitable[ResultT]]) -> Callable[P, Awaitable[ResultT]]: ...  # type: ignore[misc]

    @classmethod  # type: ignore[misc]
    def check_wallet(
        cls, wrapped_function: Callable[P, Awaitable[ResultT]] | Callable[P, ResultT]
    ) -> Callable[P, Awaitable[ResultT]] | Callable[P, ResultT]:
        if iscoroutinefunction(wrapped_function):

            @wraps(wrapped_function)
            async def async_impl(*args: P.args, **kwrags: P.kwargs) -> ResultT:
                this: WalletCommons[AsyncRemoteBeekeeper, AsyncWalletLocked] = args[0]  # type: ignore
                await this._async_call_callback_if_locked(wallet_name=this.name, token=this.session_token)
                return await wrapped_function(*args, **kwrags)  # type: ignore[no-any-return]

            return async_impl

        @wraps(wrapped_function)
        def sync_impl(*args: P.args, **kwrags: P.kwargs) -> ResultT:
            this: WalletCommons[SyncRemoteBeekeeper, SyncWalletLocked] = args[0]  # type: ignore
            this._sync_call_callback_if_locked(wallet_name=this.name, token=this.session_token)
            return wrapped_function(*args, **kwrags)  # type: ignore[return-value]

        return sync_impl
