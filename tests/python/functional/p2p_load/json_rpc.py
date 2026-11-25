"""
Plain JSON-RPC calls to a node: the driver runs with the test-tools of its image, whose typed API may not match what
the node under test returns (network_node_api.add_node's empty result fails it), so it calls these directly.
"""

from __future__ import annotations

import json
import urllib.request
from typing import Any


class JsonRpcError(RuntimeError):
    """The node answered the call with an error."""


def call(url: str, method: str, params: dict[str, Any], timeout: float = 10) -> Any:
    """Return the result of `method` called with `params` on the node at `url`; raise JsonRpcError on its error."""
    request = {"jsonrpc": "2.0", "id": 1, "method": method, "params": params}
    with urllib.request.urlopen(
        urllib.request.Request(url, data=json.dumps(request).encode(), headers={"Content-Type": "application/json"}),
        timeout=timeout,
    ) as response:
        reply = json.loads(response.read())
    if "error" in reply:
        raise JsonRpcError(f"{method} on {url} failed: {json.dumps(reply['error'])[:1000]}")
    return reply["result"]
