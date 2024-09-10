from __future__ import annotations

from beekeepy.exceptions.base import BeekeeperExecutableError, BeekeepyError, DetectableError, SchemaDetectableError
from beekeepy.exceptions.common import (
    BeekeeperAlreadyRunningError,
    BeekeeperIsNotRunningError,
    InvalidWalletNameError,
    NotPositiveTimeError,
    TimeoutReachWhileCloseError,
    TimeTooBigError,
    UnknownDecisionPathError,
    WalletIsLockedError,
)
from beekeepy.exceptions.detectable import (
    InvalidAccountNameError,
    InvalidPasswordError,
    InvalidPrivateKeyError,
    InvalidPublicKeyError,
    InvalidSchemaHexError,
    InvalidSchemaPrivateKeyError,
    InvalidSchemaPublicKeyError,
    InvalidWalletError,
    MissingSTMPrefixError,
    NotExistingKeyError,
    NoWalletWithSuchNameError,
    WalletWithSuchNameAlreadyExistsError,
)

__all__ = [
    "BeekeeperAlreadyRunningError",
    "BeekeeperExecutableError",
    "BeekeeperIsNotRunningError",
    "BeekeepyError",
    "DetectableError",
    "InvalidAccountNameError",
    "InvalidPasswordError",
    "InvalidPrivateKeyError",
    "InvalidPublicKeyError",
    "InvalidSchemaHexError",
    "InvalidSchemaPrivateKeyError",
    "InvalidSchemaPublicKeyError",
    "InvalidWalletError",
    "InvalidWalletNameError",
    "MissingSTMPrefixError",
    "NotExistingKeyError",
    "NotPositiveTimeError",
    "NoWalletWithSuchNameError",
    "SchemaDetectableError",
    "TimeoutReachWhileCloseError",
    "TimeTooBigError",
    "UnknownDecisionPathError",
    "WalletIsLockedError",
    "WalletWithSuchNameAlreadyExistsError",
]
