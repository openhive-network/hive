from __future__ import annotations

from typing import Final

import msgspec
import pytest
from hiveio_api.fields.hive_constants import MAX_INT64_VALUE, MIN_INT64_VALUE
from hiveio_api.fields.validation import HiveInt, Int64t, Uint64t

MAX_UINT64_VALUE: Final[int] = 2**64 - 1


def is_accepted(value: object, form: object) -> bool:
    try:
        msgspec.convert(value, form)
    except msgspec.ValidationError:
        return False
    return True


@pytest.mark.parametrize(
    ("value", "accepted"),
    [
        (10, True),
        (-10, True),
        ("20", True),
        ("-20", True),
        (2**70, True),
        ("invalid", False),
        (True, False),
        (1.5, False),
    ],
)
def test_hive_int_is_an_integer_also_as_a_string_of_digits(value: object, *, accepted: bool) -> None:
    # ACT & ASSERT
    assert is_accepted(value, HiveInt) is accepted


@pytest.mark.parametrize(
    ("value", "accepted"),
    [
        (MAX_INT64_VALUE, True),
        (MIN_INT64_VALUE, True),
        (0, True),
        (str(MAX_INT64_VALUE), True),
        (str(MIN_INT64_VALUE), True),
        (MAX_INT64_VALUE + 1, False),
        (MIN_INT64_VALUE - 1, False),
        ("1" * 20, False),
        ("x", False),
    ],
)
def test_int64t_takes_integers_of_64_bits(value: object, *, accepted: bool) -> None:
    # ACT & ASSERT
    assert is_accepted(value, Int64t) is accepted


@pytest.mark.parametrize(
    ("value", "accepted"),
    [
        (MAX_UINT64_VALUE, True),
        (0, True),
        (str(MAX_UINT64_VALUE), True),
        (-12, False),
        ("-12", False),
        ("1" * 21, False),
        # limitation: msgspec takes no bounds beyond int64 - values above the maximum are accepted
        (MAX_UINT64_VALUE + 1, True),
        ("9" * 20, True),
    ],
)
def test_uint64t_takes_unsigned_integers_of_64_bits(value: object, *, accepted: bool) -> None:
    # ACT & ASSERT
    assert is_accepted(value, Uint64t) is accepted
