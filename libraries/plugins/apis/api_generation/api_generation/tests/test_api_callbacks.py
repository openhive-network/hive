from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api._apis.abc.api import AbstractAsyncApi, AbstractSyncApi  # noqa: E402
from hiveio_api._apis.abc.api_collection import AbstractAsyncApiCollection, AbstractSyncApiCollection  # noqa: E402
from hiveio_api._apis.abc.sendable import AsyncSendable, SyncSendable  # noqa: E402
from hiveio_api._communication.abc.communicator_models import AsyncCallbacks, Callbacks  # noqa: E402
from hiveio_api._communication.jsonrpc import ExpectResultT, JSONRPCResult, build_response  # noqa: E402

if TYPE_CHECKING:
    from hiveio_api._communication.abc.communicator_models import Request, RequestContext


def result(expected_type: type[ExpectResultT]) -> JSONRPCResult[ExpectResultT]:
    response = build_response(expected_type, {"id": 0, "jsonrpc": "2.0", "result": "ok"})
    assert isinstance(response, JSONRPCResult)
    return response


class SyncOwner(SyncSendable):
    """Owner of APIs which keeps callbacks of the last request."""

    def __init__(self) -> None:
        self.callbacks: Callbacks | None = None

    def is_testnet(self) -> bool:
        return True

    def _send(
        self, *, expected_type: type[ExpectResultT], callbacks: Callbacks | None = None, **_: object
    ) -> JSONRPCResult[ExpectResultT]:
        self.callbacks = callbacks
        return result(expected_type)


class AsyncOwner(AsyncSendable):
    """Owner of APIs which keeps callbacks of the last request."""

    def __init__(self) -> None:
        self.callbacks: AsyncCallbacks | None = None

    def is_testnet(self) -> bool:
        return True

    async def _async_send(
        self, *, expected_type: type[ExpectResultT], callbacks: AsyncCallbacks | None = None, **_: object
    ) -> JSONRPCResult[ExpectResultT]:
        self.callbacks = callbacks
        return result(expected_type)


class SyncTestApi(AbstractSyncApi):
    endpoint_jsonrpc = AbstractSyncApi.endpoint_jsonrpc
    endpoint_rest = AbstractSyncApi.endpoint_rest()

    @classmethod
    def _register_api(cls) -> bool:
        return False

    @endpoint_jsonrpc
    def first(self, *, value: int = 0) -> str:
        raise NotImplementedError

    @endpoint_jsonrpc
    def second(self) -> str:
        raise NotImplementedError

    @endpoint_rest.get("/rest")
    def rest(self) -> str:
        raise NotImplementedError


class AsyncTestApi(AbstractAsyncApi):
    endpoint_jsonrpc = AbstractAsyncApi.endpoint_jsonrpc
    endpoint_rest = AbstractAsyncApi.endpoint_rest()

    @classmethod
    def _register_api(cls) -> bool:
        return False

    @endpoint_jsonrpc
    async def first(self, *, value: int = 0) -> str:
        raise NotImplementedError

    @endpoint_rest.get("/rest")
    async def rest(self) -> str:
        raise NotImplementedError


class SyncTestCollection(AbstractSyncApiCollection):
    def __init__(self, owner: SyncSendable) -> None:
        super().__init__(owner)
        self.test_api = SyncTestApi(owner=owner)


class AsyncTestCollection(AbstractAsyncApiCollection):
    def __init__(self, owner: AsyncSendable) -> None:
        super().__init__(owner)
        self.test_api = AsyncTestApi(owner=owner)


def prepare_request(request: Request, /) -> Request:
    return request


def other_prepare_request(request: Request, /) -> Request:
    return request


def process_response(context: RequestContext, /) -> None:
    pass


def other_process_response(context: RequestContext, /) -> None:
    pass


def test_callbacks_of_collection_api_and_endpoint_are_sent_in_this_order() -> None:
    # ARRANGE
    owner = SyncOwner()
    collection = SyncTestCollection(owner)
    collection.callbacks.prepare_request.append(prepare_request)
    collection.test_api.callbacks.process_response.append(process_response)
    collection.test_api.first.callbacks.process_response.append(other_process_response)
    collection.test_api.first.callbacks.prepare_request.append(other_prepare_request)

    # ACT
    assert collection.test_api.first(value=1) == "ok"

    # ASSERT
    assert owner.callbacks == Callbacks(
        prepare_request=[prepare_request, other_prepare_request],
        process_response=[process_response, other_process_response],
    )


def test_callbacks_of_endpoints_are_separate() -> None:
    # ARRANGE
    owner = SyncOwner()
    collection = SyncTestCollection(owner)
    other_collection = SyncTestCollection(owner)
    collection.test_api.first.callbacks.process_response.append(process_response)

    # ACT & ASSERT
    collection.test_api.second()
    assert owner.callbacks == Callbacks(), "callbacks of an endpoint are not sent with requests of other endpoints"
    other_collection.test_api.first()
    assert owner.callbacks == Callbacks(), "callbacks of an endpoint are not shared by instances of the API"
    collection.test_api.first()
    assert owner.callbacks == Callbacks(process_response=[process_response])


def test_api_outside_of_a_collection_sends_its_callbacks() -> None:
    # ARRANGE
    owner = SyncOwner()
    api = SyncTestApi(owner=owner)
    api.callbacks.prepare_request.append(prepare_request)
    api.first.callbacks.prepare_request.append(other_prepare_request)

    # ACT
    api.first()

    # ASSERT
    assert owner.callbacks == Callbacks(prepare_request=[prepare_request, other_prepare_request])


def test_rest_endpoint_sends_callbacks() -> None:
    # ARRANGE
    owner = SyncOwner()
    collection = SyncTestCollection(owner)
    collection.callbacks.process_response.append(process_response)
    collection.test_api.rest.callbacks.process_response.append(other_process_response)

    # ACT
    assert collection.test_api.rest() == "ok"

    # ASSERT
    assert owner.callbacks == Callbacks(process_response=[process_response, other_process_response])


@pytest.mark.parametrize("endpoint", ["first", "rest"])
def test_async_endpoints_send_callbacks(endpoint: str) -> None:
    # ARRANGE
    owner = AsyncOwner()
    collection = AsyncTestCollection(owner)
    api_endpoint = collection.test_api.first if endpoint == "first" else collection.test_api.rest
    collection.callbacks.prepare_request.append(prepare_request)
    collection.test_api.callbacks.process_response.append(process_response)
    api_endpoint.callbacks.process_response.append(other_process_response)

    # ACT
    assert asyncio.run(api_endpoint()) == "ok"

    # ASSERT
    assert owner.callbacks == AsyncCallbacks(
        prepare_request=[prepare_request], process_response=[process_response, other_process_response]
    )


def test_bound_endpoint_is_described_as_its_method() -> None:
    # ARRANGE
    owner = SyncOwner()
    api = SyncTestApi(owner=owner)

    # ACT
    endpoint = api.first

    # ASSERT
    assert endpoint is api.first, "endpoint is bound once per instance of the API"
    assert endpoint.__self__ is api
    assert endpoint.__name__ == "first"
    assert endpoint.__qualname__ == "SyncTestApi.first"
    assert SyncTestApi.first(api, value=1) == "ok", "endpoint can be called through the class"
