"""
This file contains global constants closely related to the behavior of the blockchain, its configuration and specific
rules prevailing in the hive network, used by the field types. e.g.:
 - the format of time,
 - the rules of names of accounts and keys,
 - the maximum length of a witness URL,
 - the ranges of integers of 64 bits.
"""

from __future__ import annotations

from typing import Final

HIVE_TIME_FORMAT: Final[str] = "%Y-%m-%dT%H:%M:%S"
HIVE_TIME_PATTERN: Final[str] = r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$"
"""Regular expression matching times in `HIVE_TIME_FORMAT` - used to validate them without parsing."""

BASE_58_REGEX: Final[str] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
"""Characters of base58 (keys) - to be put in a character class of a regular expression."""
ACCOUNT_NAME_SEGMENT_REGEX: Final[str] = r"[a-z]{1}[a-z0-9\-]+[a-z0-9]{1}"
ACCOUNT_NAME_REGEX: Final[str] = "^" + ACCOUNT_NAME_SEGMENT_REGEX + r"(?:\.{1}" + ACCOUNT_NAME_SEGMENT_REGEX + ")*$"
ACCOUNT_NAME_MIN_LENGTH: Final[int] = 3
ACCOUNT_NAME_MAX_LENGTH: Final[int] = 16

HIVE_MAX_WITNESS_URL_LENGTH: Final[int] = 2048

MAX_INT64_VALUE: Final[int] = 9223372036854775807
MIN_INT64_VALUE: Final[int] = -9223372036854775808
MIN_UINT64_VALUE: Final[int] = 0
