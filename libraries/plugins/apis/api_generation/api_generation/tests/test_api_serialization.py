"""Serialization of arguments of endpoints into `params` of JSON-RPC calls (`AbstractApi.argument_serialization`)."""

from __future__ import annotations

import json
from typing import Any

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api.client import (  # noqa: E402
    AbstractSyncApi,
    ApiArgumentSerialization,
    ExpectResultT,
    JSONRPCResult,
    SyncSendable,
    build_response,
)
from hiveio_api.fields.json_string import JsonString  # noqa: E402
from hiveio_api.fields.validation import AssetHive  # noqa: E402


class Owner(SyncSendable):
    """Owner of APIs which keeps the body of the last request."""

    def __init__(self) -> None:
        self.data: str | None = None

    def is_testnet(self) -> bool:
        return True

    def _send(
        self, *, expected_type: type[ExpectResultT], data: str | None = None, **_: object
    ) -> JSONRPCResult[ExpectResultT]:
        self.data = data
        response = build_response(expected_type, {"id": 0, "jsonrpc": "2.0", "result": None})
        assert isinstance(response, JSONRPCResult)
        return response


class ObjectApi(AbstractSyncApi):
    endpoint_jsonrpc = AbstractSyncApi.endpoint_jsonrpc

    @classmethod
    def _register_api(cls) -> bool:
        return False

    @endpoint_jsonrpc
    def transfer(self, *, from_: str, to: str, amount: int) -> None:
        raise NotImplementedError


class ArrayApi(AbstractSyncApi):
    endpoint_jsonrpc = AbstractSyncApi.endpoint_jsonrpc

    @classmethod
    def _register_api(cls) -> bool:
        return False

    def argument_serialization(self) -> ApiArgumentSerialization:
        return ApiArgumentSerialization.ARRAY

    @endpoint_jsonrpc
    def get_accounts(self, accounts: list[str], delayed_votes_active: bool, /) -> None:
        raise NotImplementedError


class DoubleArrayApi(ArrayApi):
    def argument_serialization(self) -> ApiArgumentSerialization:
        return ApiArgumentSerialization.DOUBLE_ARRAY


def sent_params(owner: Owner) -> object:
    assert owner.data is not None
    return json.loads(owner.data)["params"]


def test_keyword_arguments_are_sent_as_object_with_keys_of_the_api() -> None:
    # ARRANGE
    owner = Owner()

    # ACT
    ObjectApi(owner).transfer(from_="alice", to="bob", amount=1)

    # ASSERT - the trailing `_` of names clashing with keywords is stripped
    assert sent_params(owner) == {"from": "alice", "to": "bob", "amount": 1}


def test_validating_hive_types_are_sent_in_their_json_form() -> None:
    # ARRANGE - `json` serializes neither msgspec models (assets) nor `Serializable` types, the codec of hiveio_api does
    owner = Owner()
    memo = JsonString('{"app": "test"}')
    amount: Any = {"asset": AssetHive(amount="1000", precision=3, nai="@@000000021"), "memo": memo}

    # ACT
    ObjectApi(owner).transfer(from_="alice", to="bob", amount=amount)

    # ASSERT
    assert sent_params(owner) == {
        "from": "alice",
        "to": "bob",
        "amount": {"asset": {"amount": "1000", "precision": 3, "nai": "@@000000021"}, "memo": memo.serialize()},
    }
    assert json.loads(memo.serialize()) == {"app": "test"}


@pytest.mark.parametrize(
    ("api", "params"),
    [
        (ArrayApi, [["alice"], True]),
        (DoubleArrayApi, [[["alice"], True]]),
    ],
)
def test_positional_arguments_are_sent_as_array(api: type[ArrayApi], params: list[object]) -> None:
    # ARRANGE
    owner = Owner()

    # ACT
    api(owner).get_accounts(["alice"], True)

    # ASSERT
    assert sent_params(owner) == params


def test_api_of_object_serialization_rejects_positional_arguments() -> None:
    # ARRANGE
    owner = Owner()

    # ACT & ASSERT
    with pytest.raises(TypeError, match="only keyword arguments"):
        ObjectApi(owner).transfer("alice", to="bob", amount=1)  # type: ignore[call-arg]  # the call checked at runtime
    assert owner.data is None


def test_api_of_array_serialization_rejects_keyword_arguments() -> None:
    # ARRANGE
    owner = Owner()

    # ACT & ASSERT
    with pytest.raises(TypeError, match="only positional arguments"):
        ArrayApi(owner).get_accounts(accounts=["alice"], delayed_votes_active=True)  # type: ignore[call-arg]  # as above
    assert owner.data is None
