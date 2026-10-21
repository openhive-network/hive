from __future__ import annotations

import warnings
from dataclasses import dataclass, field

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from api_client_generator.hive_variants import HIVE_MODEL_METHODS  # noqa: E402
from api_client_generator.model_options import ALIAS_METADATA_KEY  # noqa: E402
from hiveio_api import _base  # noqa: E402
from hiveio_api._base import HiveModel, InvalidValueWarning, MissingFieldWarning, UnexpectedFieldWarning  # noqa: E402

from schemas.fields.public import HiveInt, Int64t, Uint64t  # noqa: E402  # annotations of models


@dataclass(frozen=True, kw_only=True)
class NaiAsset:
    amount: int | str
    precision: int
    nai: str


@dataclass(kw_only=True)
class Vote(HiveModel):
    voter: str
    weight: int


@dataclass(kw_only=True)
class Post(HiveModel):
    author: str
    from_: str = field(metadata={"alias": "from"})
    payout: NaiAsset
    votes: list[Vote]
    votes_by_voter: dict[str, Vote]
    parent: Vote | None = None
    title: str | None = None
    block_stats: Vote | None = field(default=None, metadata={"alias": "Block stats"})


def post_data(**overrides: object) -> dict[str, object]:
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


def test_fields_with_aliases_are_built_from_original_keys() -> None:
    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        post = Post.from_builtins(post_data(**{"Block stats": {"voter": "erin", "weight": 2}}))

    # ASSERT
    assert post.from_ == "bob"
    assert post.block_stats == Vote(voter="erin", weight=2)
    assert post.to_builtins()["Block stats"] == {"voter": "erin", "weight": 2}


def test_undeclared_fields_are_available_as_attributes() -> None:
    # ACT
    with pytest.warns(UnexpectedFieldWarning, match="new_field"):
        post = Post.from_builtins(post_data(new_field=1.5, json="clash with method"))

    # ASSERT
    assert post.new_field == 1.5  # type: ignore[attr-defined]
    assert post.json_ == "clash with method"  # type: ignore[attr-defined]
    assert post.to_builtins()["new_field"] == 1.5


def test_undeclared_fields_clashing_with_attributes_keep_their_keys() -> None:
    # ARRANGE - `json` clashes with the method, `json_` with the attribute `json` is stored in
    data = post_data(json=1, json_=2, to_builtins=3)

    # ACT
    post = Post.from_builtins(data, warn=False)

    # ASSERT
    assert (post.json_, post.json__, post.to_builtins_) == (1, 2, 3)  # type: ignore[attr-defined]
    assert post.to_builtins() == data
    assert (post["json"], post["json_"]) == (1, 2)


def test_warnings_about_nested_models_point_at_the_caller() -> None:
    # ARRANGE
    data = post_data(votes=[{"voter": "carol", "weight": 100, "unexpected": 1}])

    # ACT
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        Post.from_builtins(data)

    # ASSERT
    assert [(warning.category, warning.filename) for warning in caught] == [(UnexpectedFieldWarning, __file__)]


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
    assert post.to_builtins() == data


def test_models_are_parsed_from_their_json_and_converted_to_dicts() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data())

    # ACT
    parsed = Post.parse_raw(post.json())

    # ASSERT
    assert parsed == post
    assert parsed.dict() == post.to_builtins() == post_data()


@pytest.mark.parametrize("raw", ["[1, 2]", "123", '"text"'])
def test_json_of_models_has_to_be_an_object(raw: str) -> None:
    # ACT & ASSERT
    with pytest.raises(TypeError):
        Post.parse_raw(raw)


def test_json_keeps_explicit_nulls_and_omits_missing_fields() -> None:
    # ARRANGE
    data = post_data(title=None, parent=None)
    del data["author"]

    # ACT
    post = Post.from_builtins(data, warn=False)

    # ASSERT
    assert post.to_builtins() == data


def test_json_of_directly_created_model_omits_none_optionals() -> None:
    # ARRANGE
    post = Post(
        author="alice",
        from_="bob",
        payout=NaiAsset(amount="1", precision=3, nai="@@000000021"),
        votes=[],
        votes_by_voter={},
    )

    # ACT
    result = post.to_builtins()

    # ASSERT
    assert result == {
        "author": "alice",
        "from": "bob",
        "payout": {"amount": "1", "precision": 3, "nai": "@@000000021"},
        "votes": [],
        "votes_by_voter": {},
    }


def test_hive_model_exposes_only_conversions_and_copy() -> None:
    # ACT
    public = {name for name in dir(HiveModel) if not name.startswith("_")}

    # ASSERT - the generator renames fields of these names (`json_`)
    assert public == HIVE_MODEL_METHODS


def test_models_read_aliases_from_the_metadata_written_by_the_generator() -> None:
    # ACT & ASSERT
    assert _base._ALIAS_METADATA_KEY == ALIAS_METADATA_KEY


def test_fields_are_available_by_keys_of_json_and_by_attributes() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data(**{"Block stats": {"voter": "erin", "weight": 2}}), warn=False)

    # ACT & ASSERT
    assert post["from"] == post["from_"] == "bob"
    assert post["Block stats"] == post["block_stats"] == Vote(voter="erin", weight=2)
    assert post["votes"] == [Vote(voter="carol", weight=100)]
    assert post["title"] is None


def test_undeclared_fields_are_available_by_keys() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data(new_field=5), warn=False)

    # ACT & ASSERT
    assert post["new_field"] == 5
    assert "new_field" in post


def test_fields_are_set_by_keys_of_json_and_by_attributes() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data(new_field=5), warn=False)

    # ACT
    post["from"] = "carol"
    post["author"] = "dave"
    post["new_field"] = 6

    # ASSERT
    assert post.from_ == "carol"
    assert post.author == "dave"
    assert post["new_field"] == 6
    assert post.to_builtins() == post_data(**{"from": "carol", "author": "dave", "new_field": 6})


def test_unknown_fields_are_not_set_by_keys() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data())

    # ACT & ASSERT
    with pytest.raises(KeyError):
        post["nothing"] = 1
    assert "nothing" not in post


def test_missing_key_raises_key_error() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data())

    # ACT & ASSERT
    with pytest.raises(KeyError):
        post["nothing"]
    assert "nothing" not in post
    assert "from" in post


def test_models_are_not_iterable() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data())

    # ACT & ASSERT
    with pytest.raises(TypeError, match="not iterable"):
        iter(post)  # type: ignore[call-overload]


@dataclass(kw_only=True)
class WitnessVotes(HiveModel):
    votes: HiveInt
    proxied_vsf_votes: list[Int64t]
    total_missed: int
    last_aslot: Uint64t | None = None


def test_integers_of_64_bits_sent_as_strings_are_built_as_int() -> None:
    # ARRANGE
    data = {"votes": "9000000000000000", "proxied_vsf_votes": ["1", 2], "total_missed": "3", "last_aslot": "4"}

    # ACT
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        votes = WitnessVotes.from_builtins(data)

    # ASSERT
    assert votes.votes == 9_000_000_000_000_000
    assert votes.proxied_vsf_votes == [1, 2]
    assert votes.last_aslot == 4
    assert votes.total_missed == "3", "only integers of 64 bits come as strings - other values are kept as they are"


def test_integers_not_fitting_doubles_of_json_are_sent_as_strings() -> None:
    # ARRANGE - as sent by hived
    data = {"votes": "9007199254740992", "proxied_vsf_votes": [2**53 - 1, "-9007199254740992"], "total_missed": 3}

    # ACT
    votes = WitnessVotes.from_builtins(data)

    # ASSERT
    assert votes.votes == 2**53
    assert votes.to_builtins() == data


def test_integer_of_64_bits_in_another_string_is_kept_with_warning() -> None:
    # ACT
    with pytest.warns(InvalidValueWarning, match="cannot be built as int"):
        votes = WitnessVotes.from_builtins({"votes": "many", "proxied_vsf_votes": [], "total_missed": 0})

    # ASSERT
    assert votes.votes == "many"


def test_values_of_aliases_of_integers_of_64_bits_are_built_as_int() -> None:
    # ACT
    built = _base.build(list[Int64t | None], ["9007199254740993", 2, None])

    # ASSERT
    assert built == [2**53 + 1, 2, None]


def test_missing_and_undeclared_fields_are_warned_once_per_model() -> None:
    # ARRANGE
    data = post_data(first_new=1, second_new=2)
    del data["author"], data["payout"]

    # ACT
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        Post.from_builtins(data)

    # ASSERT
    assert [(warning.category, str(warning.message)) for warning in caught] == [
        (MissingFieldWarning, "Post: fields missing in the data, set to None: `author`, `payout`"),
        (
            UnexpectedFieldWarning,
            "Post: fields not declared in the model, available as attributes: `first_new`, `second_new`",
        ),
    ]


def test_copy_is_deep_and_sets_excluded_fields_to_none() -> None:
    # ARRANGE
    post = Post.from_builtins(post_data())

    # ACT
    copied = post.copy(exclude={"title", "author"})
    copied.votes.append(Vote(voter="dave", weight=1))
    copied.votes[0].weight = 5
    copied.votes_by_voter["carol"].weight = 5

    # ASSERT
    assert post.votes == [Vote(voter="carol", weight=100)]
    assert post.votes_by_voter == {"carol": Vote(voter="carol", weight=100)}
    assert post.author == "alice"
    assert copied.author is None
    assert copied.from_ == "bob"


@dataclass(kw_only=True)
class Upvote(HiveModel):
    voter: str
    weight: int


@dataclass(kw_only=True)
class Downvote(HiveModel):
    voter: str
    penalty: int


@dataclass(kw_only=True)
class AnyVote(HiveModel):
    vote: Upvote | Downvote


@pytest.mark.parametrize(
    ("vote", "model"),
    [
        ({"voter": "alice", "weight": 1}, Upvote),
        ({"voter": "alice", "penalty": 1}, Downvote),
        ({"voter": "alice"}, Upvote),  # a tie - the first member of the union
        ({"penalty": 1, "unknown": 2}, Downvote),
    ],
)
def test_member_of_untagged_union_is_the_model_matching_most_keys(vote: dict[str, object], model: type) -> None:
    # ACT
    built = AnyVote.from_builtins({"vote": vote}, warn=False)

    # ASSERT
    assert type(built.vote) is model
    assert built.to_builtins() == {"vote": vote}


@dataclass(kw_only=True)
class Counted(HiveModel):
    __slots__ = ("_count",)  # not in `__dict__` - not a field (`to_builtins`)

    voter: str
    votes: list[Vote]

    def __post_init__(self) -> None:
        self._count = len(self.votes)

    def _after_build(self) -> None:
        self.__post_init__()


@dataclass(kw_only=True)
class CountedVotes(HiveModel):
    counted: list[Counted]


def test_after_build_hook_sets_up_state_of_models_built_from_builtins() -> None:
    # ARRANGE
    data = {"counted": [{"voter": "alice", "votes": [{"voter": "bob", "weight": 1}, {"voter": "carol", "weight": 2}]}]}

    # ACT
    built = CountedVotes.from_builtins(data)

    # ASSERT - the hook runs after the fields are built, `_count` is not a field
    assert built.counted[0]._count == 2
    assert Counted(voter="alice", votes=[])._count == 0
    assert built.to_builtins() == data
    assert _base._model_plan(Counted).after_build is True
    assert _base._model_plan(Vote).after_build is False, "models not overriding the hook do not call it"
