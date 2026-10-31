"""
Date and time of Hive - an aware `datetime` with the JSON form of Hive in UTC (`2016-03-24T16:05:00`).

`HiveDateTime` is the type of dates of public models (`hiveio_api`) and of hand-written models - validation models check
the JSON form by a pattern (`hiveio_api.fields.validation`). Being a `datetime`, it is compared, shifted by `timedelta`
(giving `HiveDateTime`) and formatted as any other date. It is created:

- `HiveDateTime("2016-03-24T16:05:00")` - from the JSON form,
- `HiveDateTime(datetime(...))` - a naive datetime is taken as UTC, an aware one keeps its time zone,
- `HiveDateTime(2016, 3, 24, 16, 5)` - from the components of the date in UTC,
- `HiveDateTime.now()` - the current time.

Dates are compared also with dates in the JSON form and with naive datetimes (taken as UTC, also when subtracted), e.g.
`properties.time > "2016-03-24T16:05:00"`. `to_builtins` (`serialize`) gives the JSON form - in UTC, in whole
seconds.

This module must stay import-light (no msgspec, no hiveio_api modules other than import-light ones) - generated public
models use it.
"""

from __future__ import annotations

import re
from datetime import UTC, date, datetime, timedelta, tzinfo
from typing import TYPE_CHECKING, Final, Self, SupportsIndex, overload

from hiveio_api.fields.hive_constants import HIVE_TIME_FORMAT, HIVE_TIME_PATTERN
from hiveio_api.fields.serializable import Serializable

if TYPE_CHECKING:
    from hiveio_api.validation.errors import FieldValidationError

__all__ = [
    "HiveDateTime",
]

_PATTERN: Final = re.compile(HIVE_TIME_PATTERN)
_JSON_LENGTH: Final[int] = len("2016-03-24T16:05:00")
_NO_OFFSET: Final = timedelta(0)
_NO_COMPONENTS: Final = (None, None, 0, 0, 0, 0, None, 0)
"""Defaults of the components after the year - a date given by a string or a datetime takes no other argument."""


class HiveDateTime(datetime, Serializable):  # Serializable - decoded by the hooks in hand-written models (for now)
    """
    Date and time of Hive - aware (UTC when not told otherwise); in JSON as `2016-03-24T16:05:00` in UTC.

    Equal also to the same date in the JSON form and to naive datetimes (taken as UTC). Hashes of these other forms
    differ from the hash of the date - do not mix them as keys.
    """

    def __new__(  # noqa: PLR0913 # components of datetime
        cls,
        value: str | datetime | int,
        month: int | None = None,
        day: int | None = None,
        hour: int = 0,
        minute: int = 0,
        second: int = 0,
        microsecond: int = 0,
        tzinfo: tzinfo | None = None,
        *,
        fold: int = 0,
    ) -> Self:
        if isinstance(value, int):
            if month is None or day is None:
                raise TypeError("A date of components needs the year, the month and the day.")
            value = datetime(value, month, day, hour, minute, second, microsecond, tzinfo, fold=fold)
        elif (month, day, hour, minute, second, microsecond, tzinfo, fold) != _NO_COMPONENTS:
            raise TypeError("Only a date of components takes more than one argument.")
        elif isinstance(value, str):
            return cls.__from_json_form(value)
        if not isinstance(value, datetime):
            raise TypeError(f"Date must be a datetime, a string or components of the date, got {type(value).__name__}.")
        date = _aware(value)
        return super().__new__(
            cls,
            date.year,
            date.month,
            date.day,
            date.hour,
            date.minute,
            date.second,
            date.microsecond,
            date.tzinfo,
            fold=date.fold,
        )

    def __reduce_ex__(self, protocol: SupportsIndex) -> tuple[type[Self], tuple[datetime]]:
        """Copied and pickled as `datetime` - the state of `datetime` is not a value of the constructor."""
        return type(self), (self.__as_datetime(),)

    def __str__(self) -> str:
        """The JSON form, e.g. `2016-03-24T16:05:00`."""
        return self.to_builtins()

    def __repr__(self) -> str:
        if self.microsecond:
            components = (self.year, self.month, self.day, self.hour, self.minute, self.second, self.microsecond)
            return f"HiveDateTime{components}"
        return f"HiveDateTime({self.to_builtins()!r})"

    def __hash__(self) -> int:
        return datetime.__hash__(self)

    def __eq__(self, other: object) -> bool:
        comparable = _comparable(other)
        return NotImplemented if comparable is None else datetime.__eq__(self, comparable)

    def __ne__(self, other: object) -> bool:
        comparable = _comparable(other)
        return NotImplemented if comparable is None else datetime.__ne__(self, comparable)

    def __lt__(self, other: date | str) -> bool:
        comparable = _comparable(other)
        return NotImplemented if comparable is None else datetime.__lt__(self, comparable)

    def __le__(self, other: date | str) -> bool:
        comparable = _comparable(other)
        return NotImplemented if comparable is None else datetime.__le__(self, comparable)

    def __gt__(self, other: date | str) -> bool:
        comparable = _comparable(other)
        return NotImplemented if comparable is None else datetime.__gt__(self, comparable)

    def __ge__(self, other: date | str) -> bool:
        comparable = _comparable(other)
        return NotImplemented if comparable is None else datetime.__ge__(self, comparable)

    @overload  # type: ignore[override]  # also naive datetimes (taken as UTC), not only of its own type
    def __sub__(self, value: timedelta, /) -> Self: ...

    @overload
    def __sub__(self, value: datetime, /) -> timedelta: ...

    def __sub__(self, value: timedelta | datetime, /) -> Self | timedelta:
        if isinstance(value, datetime):
            return datetime.__sub__(self, _aware(value))
        return datetime.__sub__(self, value)

    def __rsub__(self, value: datetime, /) -> timedelta:
        """`datetime - HiveDateTime` - a naive datetime is taken as UTC."""
        return datetime.__sub__(_aware(value), self)

    def replace(self, *args: SupportsIndex | tzinfo | None, **changes: SupportsIndex | tzinfo | None) -> Self:
        """
        The date with the given components replaced (as `datetime.replace`) - a naive one is taken as UTC.

        The result is made by the constructor on every version of Python (`datetime.replace` of Python older than 3.14
        skips the constructor of subclasses, so `replace(tzinfo=None)` gave a naive `HiveDateTime` there).
        """
        changed = self.__as_datetime().replace(*args, **changes)  # type: ignore[arg-type]  # checked by `datetime`
        return type(self)(changed)

    __replace__ = replace  # `copy.replace`

    @classmethod
    def now(cls, tz: tzinfo | None = None) -> Self:
        """The current time - in the time zone `tz`, UTC by default."""
        return cls(datetime.now(tz or UTC))

    @classmethod
    def fromtimestamp(cls, timestamp: float, tz: tzinfo | None = None) -> Self:
        """The date of a POSIX timestamp - in the time zone `tz`, UTC by default."""
        return cls(datetime.fromtimestamp(timestamp, tz or UTC))

    @classmethod
    def from_builtins(cls, value: object) -> Self:
        """
        Build the date from its JSON form (e.g. `2016-03-24T16:05:00`).

        Args:
            value: The JSON form - a string in `HIVE_TIME_FORMAT`.

        Raises:
            TypeError: When the value is not a string.
            ValueError: When the string is not in the JSON form of dates.
        """
        if not isinstance(value, str):
            raise TypeError(f"Date must be a string in format {HIVE_TIME_FORMAT}, got {type(value).__name__}.")
        return cls.__from_json_form(value)

    def to_builtins(self) -> str:
        """The JSON form, e.g. `2016-03-24T16:05:00`."""
        in_utc = self.tzinfo is UTC or self.utcoffset() == _NO_OFFSET
        return datetime.isoformat(self if in_utc else datetime.astimezone(self, UTC), timespec="seconds")[:_JSON_LENGTH]

    serialize = to_builtins  # the name of the JSON form of field types (`Serializable`)

    def __as_datetime(self) -> datetime:
        """The same date as `datetime`."""
        components = (self.year, self.month, self.day, self.hour, self.minute, self.second, self.microsecond)
        return datetime(*components, self.tzinfo, fold=self.fold)

    @classmethod
    def __from_json_form(cls, value: str) -> Self:
        # the pattern admits only the form of Hive - `fromisoformat` (C) parses it and checks the calendar
        if _PATTERN.match(value) is None:
            raise _field_validation_error(f"Date must be in format {HIVE_TIME_FORMAT}, got {value!r}.")
        try:
            date = datetime.fromisoformat(value)
        except ValueError as error:
            raise _field_validation_error(f"Date must be in format {HIVE_TIME_FORMAT}, got {value!r}.") from error
        return datetime.__new__(cls, date.year, date.month, date.day, date.hour, date.minute, date.second, 0, UTC)


def _aware(value: datetime) -> datetime:
    """The datetime with the time zone - UTC when it is naive."""
    return value.replace(tzinfo=UTC) if value.tzinfo is None else value


def _comparable(other: object) -> datetime | None:
    """The other side of a comparison as an aware datetime, `None` when it cannot be compared with dates."""
    if isinstance(other, datetime):
        return _aware(other)
    if isinstance(other, str):
        try:
            return HiveDateTime(other)
        except ValueError:
            return None
    return None


def _field_validation_error(message: str) -> FieldValidationError:
    from hiveio_api.validation.errors import FieldValidationError  # the module stays import-light (msgspec)

    return FieldValidationError(message)
