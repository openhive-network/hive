from __future__ import annotations

import json

import aiohttp
import pytest

from clive.__private.core.beekeeper import Beekeeper
from clive.core.url import Url
from clive_local_tools.beekeeper.network import get_port
from schemas.apis import beekeeper_api
from schemas.jsonrpc import get_response_model


async def check_webserver_http_endpoint(*, nofification_endpoint: Url | None, webserver_http_endpoint: Url) -> None:
    """Check if beekeeper is listening on given endpoint."""
    nofification_endpoint_url = (
        nofification_endpoint if nofification_endpoint else Url(proto="http", host="127.0.0.1", port=None)
    )

    data = {
        "jsonrpc": "2.0",
        "method": "beekeeper_api.create_session",
        "params": {
            "salt": "avocado",
            "notifications_endpoint": nofification_endpoint_url.as_string(with_protocol=False),
        },
        "id": 1,
    }

    async with aiohttp.ClientSession() as session, session.post(
        webserver_http_endpoint.as_string(), data=json.dumps(data)
    ) as resp:
        assert resp.ok
        resp_json = await resp.json()
        get_response_model(beekeeper_api.CreateSession, **resp_json)


@pytest.mark.parametrize(
    "webserver_http_endpoint",
    [
        Url(proto="http", host="0.0.0.0", port=get_port()),
        Url(proto="http", host="127.0.0.1", port=get_port()),
    ],
)
async def test_webserver_http_endpoint(webserver_http_endpoint: Url) -> None:
    """Test will check command line flag --webserver_http_endpoint."""
    # ARRANGE & ACT
    async with await Beekeeper().launch(webserver_http_endpoint=webserver_http_endpoint) as beekeeper:
        # ASSERT
        await check_webserver_http_endpoint(
            nofification_endpoint=beekeeper.notification_server_http_endpoint,
            webserver_http_endpoint=webserver_http_endpoint,
        )
