from __future__ import annotations

import json
import warnings
from dataclasses import dataclass
from datetime import timedelta
from typing import Final

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api import HiveDateTime, InvalidValueWarning  # noqa: E402
from hiveio_api._base import HiveModel, to_api_builtins  # noqa: E402
from hiveio_api.database_api import GetDynamicGlobalPropertiesResponse  # noqa: E402
from hiveio_api.operations import Transaction  # noqa: E402


@dataclass(kw_only=True)
class Votes(HiveModel):
    time: HiveDateTime
    optional: HiveDateTime | None = None
    history: list[HiveDateTime] | None = None
    subscriber: tuple[str, HiveDateTime] | None = None


DATA: Final[dict[str, object]] = {
    "time": "2016-03-24T16:05:00",
    "optional": "1970-01-01T00:00:00",
    "history": ["1969-12-31T23:59:59", "2106-02-07T06:28:15"],
    "subscriber": ["alice", "2020-02-09T18:24:57"],
}


def test_dates_are_built_as_hive_date_time() -> None:
    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        votes = Votes.from_builtins(DATA)

    # ASSERT
    assert isinstance(votes.time, HiveDateTime)
    assert votes.time == HiveDateTime(2016, 3, 24, 16, 5)
    assert votes.optional is not None
    assert votes.optional.timestamp() == 0
    assert votes.history == ["1969-12-31T23:59:59", "2106-02-07T06:28:15"]
    assert all(isinstance(date, HiveDateTime) for date in votes.history)
    assert votes.subscriber is not None
    assert isinstance(votes.subscriber[1], HiveDateTime)


def test_dates_are_serialized_back_to_the_same_form() -> None:
    # ARRANGE
    votes = Votes.from_builtins(DATA)

    # ACT & ASSERT
    assert votes.to_builtins() == DATA
    assert json.loads(votes.json()) == DATA


def test_value_which_is_not_a_date_is_kept_with_a_warning() -> None:
    # ACT
    with pytest.warns(InvalidValueWarning, match="`'2016-03-24T16:05:00Z'` cannot be built as HiveDateTime"):
        votes = Votes.from_builtins({**DATA, "time": "2016-03-24T16:05:00Z"})

    # ASSERT
    assert votes.time == "2016-03-24T16:05:00Z"
    assert votes.to_builtins()["time"] == "2016-03-24T16:05:00Z"


def test_dates_in_parameters_are_sent_in_the_json_form() -> None:
    # ARRANGE
    votes = Votes(time=HiveDateTime("2016-03-24T16:05:00") + timedelta(minutes=1))

    # ACT
    params = to_api_builtins({"votes": votes, "start": HiveDateTime(2020, 1, 1)}, testnet=lambda: False)

    # ASSERT
    assert params == {"votes": {"time": "2016-03-24T16:06:00"}, "start": "2020-01-01T00:00:00"}


def test_dates_of_generated_models_are_hive_date_time() -> None:
    # ACT
    properties = GetDynamicGlobalPropertiesResponse.from_builtins({"time": "2016-03-24T16:05:00"}, warn=False)
    transaction = Transaction.from_builtins({"expiration": "2016-03-24T16:15:00"}, warn=False)

    # ASSERT
    assert isinstance(properties.time, HiveDateTime)
    assert isinstance(transaction.expiration, HiveDateTime)
    assert transaction.expiration - properties.time == timedelta(minutes=10)
