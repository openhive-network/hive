from __future__ import annotations

import json
from typing import TYPE_CHECKING

import pytest
import requests
from beekeepy._handle.beekeeper import Beekeeper
from beekeepy.settings import Settings
from helpy import HttpUrl

from hive_local_tools.beekeeper.network import get_port
from schemas.apis import beekeeper_api
from schemas.jsonrpc import get_response_model

if TYPE_CHECKING:
    from hive_local_tools.beekeeper.models import SettingsLoggerFactory


def check_webserver_http_endpoint(*, nofification_endpoint: HttpUrl, webserver_http_endpoint: HttpUrl) -> None:
    """Check if beekeeper is listening on given endpoint."""
    data = {
        "jsonrpc": "2.0",
        "method": "beekeeper_api.create_session",
        "params": {
            "salt": "avocado",
            "notifications_endpoint": nofification_endpoint.as_string(with_protocol=False),
        },
        "id": 1,
    }

    resp = requests.post(webserver_http_endpoint.as_string(), data=json.dumps(data))
    assert resp.ok
    resp_json = resp.json()
    get_response_model(beekeeper_api.CreateSession, **resp_json)


@pytest.mark.parametrize(
    "webserver_http_endpoint",
    [
        HttpUrl(f"0.0.0.0:{get_port()}", protocol="http"),
        HttpUrl(f"127.0.0.1:{get_port()}", protocol="http"),
    ],
)
def test_webserver_http_endpoint(settings_with_logger: SettingsLoggerFactory, webserver_http_endpoint: HttpUrl) -> None:
    """Test will check command line flag --webserver_http_endpoint."""
    # ARRANGE & ACT
    settings, logger = settings_with_logger(Settings(http_endpoint=webserver_http_endpoint))
    with Beekeeper(settings=settings, logger=logger) as beekeeper:
        # ASSERT
        assert beekeeper.settings.notification_endpoint is not None
        check_webserver_http_endpoint(
            nofification_endpoint=beekeeper.settings.notification_endpoint,
            webserver_http_endpoint=webserver_http_endpoint,
        )
