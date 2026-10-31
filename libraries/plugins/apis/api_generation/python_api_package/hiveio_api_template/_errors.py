"""Exceptions of the package."""

from __future__ import annotations

__all__ = [
    "HiveioApiBaseError",
    "UnknownEndpointError",
    "UnsupportedTypeError",
]


class HiveioApiBaseError(Exception):
    """Base class of all exceptions defined in the package."""


class UnknownEndpointError(HiveioApiBaseError):
    """Raised when there is no validation model for the given endpoint."""


class UnsupportedTypeError(HiveioApiBaseError):
    """Raised when a type cannot be built from builtins without validation (e.g. a msgspec model)."""
