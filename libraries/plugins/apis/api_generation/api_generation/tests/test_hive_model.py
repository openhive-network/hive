from __future__ import annotations

import dataclasses
import importlib.util
import json
import sys
import warnings
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import pytest

_BASE_MODULE_PATH = Path(__file__).parents[2] / "python_api_package" / "hiveio_api" / "_base.py"
_spec = importlib.util.spec_from_file_location("hive_model_under_test", _BASE_MODULE_PATH)
assert _spec is not None
assert _spec.loader is not None
_base = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = _base
_spec.loader.exec_module(_base)

HiveModel = _base.HiveModel
MissingFieldWarning = _base.MissingFieldWarning
UnexpectedFieldWarning = _base.UnexpectedFieldWarning


@dataclass(frozen=True, kw_only=True)
class NaiAsset:
    amount: int | str
    precision: int
    nai: str


@dataclass(frozen=True, kw_only=True)
class Vote(HiveModel):
    voter: str
    weight: int


@dataclass(frozen=True, kw_only=True)
class Post(HiveModel):
    author: str
    from_: str
    payout: NaiAsset
    votes: list[Vote]
    votes_by_voter: dict[str, Vote]
    parent: Vote | None = None
    title: str | None = None


def post_data(**overrides: Any) -> dict[str, Any]:
    return {
        "author": "alice",
        "from": "bob",
        "payout": {"amount": "1000", "precision": 3, "nai": "@@000000021"},
        "votes": [{"voter": "carol", "weight": 100}],
        "votes_by_voter": {"carol": {"voter": "carol", "weight": 100}},
        **overrides,
    }


def test_builds_nested_models_without_warnings() -> None:
    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        post = Post.from_builtins(post_data(parent={"voter": "dave", "weight": 1}))

    # ASSERT
    assert post.from_ == "bob"
    assert post.payout == NaiAsset(amount="1000", precision=3, nai="@@000000021")
    assert post.votes == [Vote(voter="carol", weight=100)]
    assert post.votes_by_voter["carol"] == Vote(voter="carol", weight=100)
    assert post.parent == Vote(voter="dave", weight=1)
    assert post.title is None


def test_undeclared_fields_are_available_as_attributes() -> None:
    # ACT
    with pytest.warns(UnexpectedFieldWarning, match="new_field"):
        post = Post.from_builtins(post_data(new_field=1.5, json="clash with method"))

    # ASSERT
    assert post.new_field == 1.5  # type: ignore[attr-defined]
    assert post.json_ == "clash with method"  # type: ignore[attr-defined]
    assert json.loads(post.json())["new_field"] == 1.5


def test_missing_required_field_is_none_and_warned() -> None:
    # ARRANGE
    data = post_data()
    del data["author"]

    # ACT
    with pytest.warns(MissingFieldWarning, match="author"):
        post = Post.from_builtins(data)

    # ASSERT
    assert post.author is None


def test_missing_optional_field_is_not_warned() -> None:
    # ACT & ASSERT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        Post.from_builtins(post_data())


def test_warnings_can_be_disabled() -> None:
    # ARRANGE
    data = post_data(new_field=1)
    del data["author"]

    # ACT & ASSERT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        Post.from_builtins(data, warn=False)


def test_values_are_not_validated() -> None:
    # ACT
    post = Post.from_builtins(post_data(author=123, payout="1.000 HIVE", votes=None), warn=False)

    # ASSERT
    assert post.author == 123
    assert post.payout == "1.000 HIVE"
    assert post.votes is None


def test_json_round_trip_keeps_original_shape() -> None:
    # ARRANGE
    data = post_data(new_field={"nested": [1, 2]})

    # ACT
    post = Post.from_builtins(data, warn=False)

    # ASSERT
    assert json.loads(post.json()) == data


def test_models_are_frozen() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data())

    # ACT & ASSERT
    with pytest.raises(dataclasses.FrozenInstanceError):
        post.author = "mallory"  # type: ignore[misc]


def test_hive_model_exposes_only_json_and_from_builtins() -> None:
    # ACT
    public = {name for name in dir(HiveModel) if not name.startswith("_")}

    # ASSERT
    assert public == {"json", "from_builtins"}
