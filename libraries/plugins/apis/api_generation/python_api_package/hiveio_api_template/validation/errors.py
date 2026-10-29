"""Exceptions of the field types of hiveio_api and aliases of msgspec exceptions used throughout the project."""

from __future__ import annotations

from msgspec import DecodeError, ValidationError

__all__ = ["DecodeError", "FieldValidationError", "SchemaBaseError", "ValidationError"]


class SchemaBaseError(Exception):
    """Base class of all exceptions of the field types of hiveio_api."""


class FieldValidationError(SchemaBaseError, ValidationError, ValueError):
    """
    Raised by Hive field types (`AccountName`, `Uint16t`, `HiveDateTime`, ...) when a value does not match the type.

    It is a `ValidationError`, so invalid data is reported with one exception type, whether it was found by msgspec
    or by a field validator. It is also a `ValueError` - msgspec adds the location (`- at $.path`) only to
    `ValueError`/`TypeError` raised by validators and wraps them in its own `ValidationError`.
    """
