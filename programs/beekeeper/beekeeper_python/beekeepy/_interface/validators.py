from __future__ import annotations

import re
from typing import TYPE_CHECKING, Final, Pattern

from beekeepy._interface.exceptions import (
    InvalidAccountNameError,
    InvalidSchemaPrivateKeyError,
    InvalidSchemaPublicKeyError,
    InvalidWalletNameError,
    NotPositiveTimeError,
    SchemaDetectableError,
)
from schemas.fields.basic import AccountName, PrivateKey, PublicKey

if TYPE_CHECKING:
    from pydantic import ConstrainedStr

__all__ = ["validate_account_names", "validate_private_keys", "validate_public_keys"]
wallet_name_regex: Final[Pattern[str]] = re.compile(r"^[a-zA-Z0-9_\._\-\@]+$")


def _generic_kwargs_validator(
    arguments: dict[str, str], exception: type[SchemaDetectableError], validator: type[ConstrainedStr]
) -> None:
    for arg_name, arg_value in arguments.items():
        with exception(arg_name=arg_name, arg_value=arg_value):
            validator.validate(arg_value)


def validate_account_names(**kwargs: str) -> None:
    _generic_kwargs_validator(kwargs, InvalidAccountNameError, AccountName)


def validate_private_keys(**kwargs: str) -> None:
    _generic_kwargs_validator(kwargs, InvalidSchemaPrivateKeyError, PrivateKey)


def validate_public_keys(**kwargs: str) -> None:
    _generic_kwargs_validator(kwargs, InvalidSchemaPublicKeyError, PublicKey)


def validate_seconds(time: int) -> None:
    if time <= 0:
        raise NotPositiveTimeError(time=time)


def validate_wallet_name(wallet_name: str) -> None:
    if wallet_name_regex.match(wallet_name) is None:
        raise InvalidWalletNameError(wallet_name=wallet_name)
