from __future__ import annotations

from beekeepy.exceptions.base import BeekeepyError
from beekeepy.exceptions.common import (
    BeekeeperAlreadyRunningError,
    BeekeeperExecutableError,
    BeekeeperIsNotRunningError,
    InvalidWalletNameError,
    NotPositiveTimeError,
    TimeoutReachWhileCloseError,
    UnknownDecisionPathError,
    WalletIsLockedError,
)
from beekeepy.exceptions.detectable import (
    InvalidAccountNameError,
    InvalidPasswordError,
    InvalidPrivateKeyError,
    InvalidPublicKeyError,
    InvalidSchemaPrivateKeyError,
    InvalidSchemaPublicKeyError,
    InvalidWalletError,
    MissingSTMPrefixError,
    NoWalletWithSuchNameError,
    RemovingNotExistingKeyError,
    SchemaDetectableError,
    WalletWithSuchNameAlreadyExistsError,
)

__all__ = [
    "BeekeeperAlreadyRunningError",
    "BeekeeperExecutableError",
    "BeekeeperIsNotRunningError",
    "BeekeepyError",
    "InvalidAccountNameError",
    "InvalidPasswordError",
    "InvalidPrivateKeyError",
    "InvalidPublicKeyError",
    "InvalidSchemaPrivateKeyError",
    "InvalidSchemaPublicKeyError",
    "InvalidWalletError",
    "InvalidWalletNameError",
    "MissingSTMPrefixError",
    "NotPositiveTimeError",
    "NoWalletWithSuchNameError",
    "RemovingNotExistingKeyError",
    "SchemaDetectableError",
    "TimeoutReachWhileCloseError",
    "UnknownDecisionPathError",
    "WalletIsLockedError",
    "WalletWithSuchNameAlreadyExistsError",
]
