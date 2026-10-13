from __future__ import annotations

import threading
from typing import Any

import pytest
import requests

import test_tools as tt


@pytest.mark.parametrize(
    "method",
    [
        "condenser_api.broadcast_transaction_synchronous",
        "wallet_bridge_api.broadcast_transaction_synchronous",
    ],
)
def test_node_exits_while_synchronous_broadcast_waits_for_block(method: str) -> None:
    """SIGINT must close the node even when a synchronous broadcast still waits for its block."""
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)
    network.run()

    # condenser_api accepts only the legacy transaction format, which is what OldWallet produces.
    wallet = tt.OldWallet(attach_to=init_node)
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)

    # The API node needs the block that the transaction refers to (TaPoS).
    api_node.wait_for_block_with_number(init_node.get_last_block_number())
    # Without the block producer no block will include the transaction, so the call waits until shutdown.
    init_node.close()

    # A raw request, because the test-tools client times out and retries, which would send the transaction again.
    # wallet_bridge_api takes all its arguments as one array.
    params = [[transaction]] if method.startswith("wallet_bridge_api.") else [transaction]
    outcome: dict[str, Any] = {}

    def broadcast_synchronously() -> None:
        try:
            outcome["response"] = requests.post(
                api_node.http_endpoint.as_string(),
                json={"jsonrpc": "2.0", "method": method, "params": params, "id": 1},
                timeout=60,
            ).json()
        except requests.RequestException as exception:
            outcome["exception"] = exception

    broadcasting_thread = threading.Thread(target=broadcast_synchronously)
    broadcasting_thread.start()

    tt.Time.wait_for(
        lambda: bool(outcome) or api_node.api.database.is_known_transaction(id=transaction["transaction_id"]).is_known,
        timeout=20,
        timeout_error_message="API node didn't accept the transaction in the allotted time",
    )
    assert not outcome, f"Synchronous broadcast finished before the node was closed: {outcome}"

    # Sends SIGINT and raises if the node doesn't exit within the close timeout.
    api_node.close()

    broadcasting_thread.join(timeout=20)
    assert not broadcasting_thread.is_alive(), "Synchronous broadcast is still waiting after the node exited"
    # A result would look like a confirmation; the transaction was accepted, but its fate is unknown.
    error_message = outcome.get("response", {}).get("error", {}).get("message", "")
    assert "Node stopped applying blocks" in error_message, f"Expected an error about the shutdown, got: {outcome}"
