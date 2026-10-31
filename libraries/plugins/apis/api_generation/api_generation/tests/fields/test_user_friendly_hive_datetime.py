from __future__ import annotations

import copy
import pickle
import sys
from datetime import UTC, date, datetime, timedelta, timezone
from typing import TYPE_CHECKING, Final

import pytest
from hiveio_api import HiveDateTime
from hiveio_api.validation.errors import FieldValidationError

if TYPE_CHECKING:
    from collections.abc import Callable

PLUS_TWO: Final = timezone(timedelta(hours=2))
MICROSECOND: Final[int] = 7


@pytest.mark.parametrize("value", ["2016-03-24T16:05:00", "1970-01-01T00:00:00", "1969-12-31T23:59:59"])
def test_dates_built_from_json_are_serialized_back_to_the_same_form(value: str) -> None:
    # ACT
    date = HiveDateTime.from_builtins(value)

    # ASSERT
    assert date.tzinfo is UTC
    assert date.to_builtins() == value
    assert date.serialize() == value
    assert str(date) == value
    assert repr(date) == f"HiveDateTime({value!r})"


@pytest.mark.parametrize("value", ["2016-3-24T16:05:00", "2016-03-24T16:05:00Z", "2016-03-24T16:05:00.000", "2016"])
def test_strings_not_in_the_json_form_are_rejected(value: str) -> None:
    # ACT & ASSERT
    with pytest.raises(FieldValidationError, match="Date must be in format"):
        HiveDateTime.from_builtins(value)


def test_values_other_than_strings_are_not_built_from_json() -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError):
        HiveDateTime.from_builtins(1458835500)


@pytest.mark.parametrize("value", [1458835500.0, None, date(2016, 3, 24), 2016])
def test_values_of_other_types_are_rejected(value: object) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError):
        HiveDateTime(value)  # type: ignore[arg-type]


@pytest.mark.parametrize(
    "create",
    [
        lambda: HiveDateTime("2016-03-24T16:05:00", 7),
        lambda: HiveDateTime("2016-03-24T16:05:00", tzinfo=PLUS_TWO),
        lambda: HiveDateTime(datetime(2016, 3, 24), 3, 24),
        lambda: HiveDateTime(datetime(2016, 3, 24), fold=1),
    ],
)
def test_date_given_by_one_value_takes_no_other_arguments(create: Callable[[], HiveDateTime]) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError, match="Only a date of components"):
        create()


def test_naive_datetime_is_taken_as_utc() -> None:
    # ACT
    date = HiveDateTime(datetime(2016, 3, 24, 16, 5))

    # ASSERT
    assert date.tzinfo is UTC
    assert date.to_builtins() == "2016-03-24T16:05:00"


def test_aware_datetime_keeps_its_time_zone_and_is_serialized_in_utc() -> None:
    # ACT
    date = HiveDateTime(datetime(2016, 3, 24, 18, 5, tzinfo=PLUS_TWO))

    # ASSERT
    assert date.tzinfo is PLUS_TWO
    assert date.to_builtins() == "2016-03-24T16:05:00"
    assert date == HiveDateTime("2016-03-24T16:05:00")


def test_date_is_created_from_components_in_utc() -> None:
    # ACT
    date = HiveDateTime(2016, 3, 24, 16, 5)

    # ASSERT
    assert date == datetime(2016, 3, 24, 16, 5, tzinfo=UTC)
    assert date.to_builtins() == "2016-03-24T16:05:00"


def test_now_is_the_current_time_in_utc() -> None:
    # ACT
    before = datetime.now(UTC)
    date = HiveDateTime.now()
    after = datetime.now(UTC)

    # ASSERT
    assert date.tzinfo is UTC
    assert before <= date <= after


def test_date_of_timestamp_is_in_utc() -> None:
    # ACT & ASSERT
    assert HiveDateTime.fromtimestamp(0).to_builtins() == "1970-01-01T00:00:00"


@pytest.mark.parametrize(
    "other",
    ["2016-03-24T16:05:00", datetime(2016, 3, 24, 16, 5), datetime(2016, 3, 24, 18, 5, tzinfo=PLUS_TWO)],
)
def test_dates_are_equal_to_the_json_form_and_to_datetimes(other: str | datetime) -> None:
    # ARRANGE
    date = HiveDateTime("2016-03-24T16:05:00")

    # ACT & ASSERT
    assert date == other
    assert other == date
    assert not date != other  # noqa: SIM202 # the operator itself is tested


def test_dates_are_ordered_with_the_json_form_and_with_naive_datetimes() -> None:
    # ARRANGE
    date = HiveDateTime("2016-03-24T16:05:00")

    # ACT & ASSERT
    assert date < "2016-03-24T16:05:01"
    assert date <= "2016-03-24T16:05:00"
    assert date > datetime(2016, 3, 24, 16, 4, 59)
    assert date >= datetime(2016, 3, 24, 16, 5)
    assert "2016-03-24T16:05:01" > date  # noqa: SIM300 # the reflected operator is tested
    assert datetime(2016, 1, 1) < date


@pytest.mark.parametrize("other", ["not a date", 1458835500, None])
def test_dates_are_not_equal_to_other_values(other: object) -> None:
    # ACT & ASSERT
    assert HiveDateTime("2016-03-24T16:05:00") != other


def test_arithmetic_with_timedelta_gives_dates() -> None:
    # ARRANGE
    date = HiveDateTime("2016-03-24T16:05:00")

    # ACT
    later = date + timedelta(hours=1)
    earlier = date - timedelta(days=1)

    # ASSERT
    assert isinstance(later, HiveDateTime)
    assert later.to_builtins() == "2016-03-24T17:05:00"
    assert isinstance(earlier, HiveDateTime)
    assert earlier.to_builtins() == "2016-03-23T16:05:00"
    assert later - date == timedelta(hours=1)


def test_date_replaced_without_time_zone_is_in_utc() -> None:
    """The same on every version of Python - `datetime.replace` of Python older than 3.14 skips the constructor."""
    # ARRANGE
    date = HiveDateTime(datetime(2016, 3, 24, 18, 5, tzinfo=PLUS_TWO))
    expected = HiveDateTime("2016-03-24T18:05:00")

    # ACT
    replaced = [date.replace(tzinfo=None)]
    if sys.version_info >= (3, 13):
        replaced.append(copy.replace(date, tzinfo=None))

    # ASSERT
    for result in replaced:
        assert type(result) is HiveDateTime
        assert result.tzinfo is UTC
        assert result.to_builtins() == "2016-03-24T18:05:00"
        assert result == expected
        assert hash(result) == hash(expected)


def test_replaced_components_keep_the_time_zone() -> None:
    # ACT
    replaced = HiveDateTime(datetime(2016, 3, 24, 18, 5, tzinfo=PLUS_TWO)).replace(hour=19, microsecond=MICROSECOND)

    # ASSERT
    assert type(replaced) is HiveDateTime
    assert replaced.tzinfo is PLUS_TWO
    assert replaced.to_builtins() == "2016-03-24T17:05:00"


def test_dates_are_hashed_as_datetimes() -> None:
    # ARRANGE
    date = HiveDateTime("2016-03-24T16:05:00")

    # ACT & ASSERT
    assert hash(date) == hash(datetime(2016, 3, 24, 16, 5, tzinfo=UTC))
    assert len({date, HiveDateTime(2016, 3, 24, 16, 5)}) == 1


def test_copies_keep_the_value_and_the_time_zone() -> None:
    # ARRANGE
    date = HiveDateTime(datetime(2016, 3, 24, 18, 5, 0, MICROSECOND, tzinfo=PLUS_TWO, fold=1))

    # ACT
    copies = [copy.copy(date), copy.deepcopy(date), pickle.loads(pickle.dumps(date))]

    # ASSERT
    for copied in copies:
        assert isinstance(copied, HiveDateTime)
        assert copied == date
        assert copied.microsecond == MICROSECOND
        assert copied.tzinfo == PLUS_TWO
        assert copied.fold == 1


def test_repr_of_date_with_fraction_of_second_shows_components() -> None:
    # ACT & ASSERT
    assert repr(HiveDateTime(2016, 3, 24, 16, 5, 0, 7)) == "HiveDateTime(2016, 3, 24, 16, 5, 0, 7)"


def test_naive_datetimes_are_subtracted_as_utc() -> None:
    # ARRANGE
    date = HiveDateTime("1970-01-01T00:05:00")
    epoch = datetime(1970, 1, 1)  # naive on purpose

    # ACT & ASSERT
    assert date - epoch == timedelta(minutes=5)
    assert epoch - date == timedelta(minutes=-5)
    assert date - HiveDateTime("1970-01-01T00:00:00") == timedelta(minutes=5)
