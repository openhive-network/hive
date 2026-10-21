from __future__ import annotations

import json
from dataclasses import FrozenInstanceError

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api._communication.abc.communicator_models import (  # noqa: E402
    Request,
    RequestContext,
    RequestTimings,
    Response,
)
from hiveio_api._communication.jsonrpc import JSONRPCRequest  # noqa: E402
from hiveio_api._communication.url import HttpUrl  # noqa: E402

URL = HttpUrl("http://127.0.0.1:8090")


def request(body: str | None) -> Request:
    return Request(url=URL, method="POST", headers={}, body=body)


def response(body: str) -> Response:
    return Response(status_code=200, headers={}, body=body)


def test_request_of_a_single_call() -> None:
    # ARRANGE
    tested = request('{"jsonrpc": "2.0", "method": "block_api.get_block", "params": {"block_num": 1}, "id": 3}')

    # ACT & ASSERT
    assert tested.json == {"jsonrpc": "2.0", "method": "block_api.get_block", "params": {"block_num": 1}, "id": 3}
    assert tested.json is tested.json, "body is parsed once"
    assert tested.jsonrpc == (JSONRPCRequest(method="block_api.get_block", params={"block_num": 1}, id_=3),)


def test_request_of_a_batch() -> None:
    # ARRANGE
    tested = request('[{"jsonrpc": "2.0", "method": "a.b", "id": 0}, {"jsonrpc": "2.0", "method": "c.d", "id": 1}]')

    # ACT & ASSERT
    assert [(call.method, call.id_) for call in tested.jsonrpc] == [("a.b", 0), ("c.d", 1)]


@pytest.mark.parametrize("body", [None, "plain text", '{"not": "a call"}'], ids=["no-body", "not-json", "not-call"])
def test_request_which_is_not_jsonrpc(body: str | None) -> None:
    # ACT & ASSERT
    assert request(body).jsonrpc == ()


def test_response_which_is_not_parsable() -> None:
    # ARRANGE
    tested = response("404: Not Found")

    # ACT & ASSERT
    with pytest.raises(json.JSONDecodeError):
        _ = tested.json


def test_requests_and_responses_are_immutable() -> None:
    # ACT & ASSERT
    with pytest.raises(FrozenInstanceError):
        request(None).body = "{}"  # type: ignore[misc]
    with pytest.raises(FrozenInstanceError):
        response("{}").body = "[]"  # type: ignore[misc]


def test_attempt_is_the_number_of_timed_attempts() -> None:
    # ARRANGE
    timings = RequestTimings()
    context = RequestContext(request=request(None), timings=timings)

    # ACT
    timings.attempts.extend([0.1, 0.2])

    # ASSERT
    assert context.attempt == 2
