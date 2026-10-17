from __future__ import annotations

import json
from typing import Any

from beekeepy.interfaces import P2PUrl

import test_tools as tt

RESTARTS = 3


def _peer_records(node: tt.ApiNode) -> dict[str, dict[str, Any]]:
    records = json.loads((node.directory / "p2p" / "peers.json").read_text())
    return {record["endpoint"]: record for record in records}


def _seed_endpoints(seed: tt.InitNode, *addresses: str) -> list[str]:
    port = seed.p2p_endpoint.port
    return [f"{address}:{port}" for address in addresses]


def test_already_connected_rejection_does_not_grow_retry_backoff() -> None:
    """
    One seed reached through two addresses (as a host with both IPv4 and IPv6 addresses is) gets two peer
    database entries; the second handshake is rejected as already connected and must not count as a failure.
    """
    seed = tt.InitNode()
    seed.run()
    endpoints = _seed_endpoints(seed, "127.0.0.1", "127.0.0.2")

    node = tt.ApiNode()
    node.config.p2p_seed_node.extend(P2PUrl(endpoint) for endpoint in endpoints)

    for run in range(RESTARTS + 1):
        if run == 0:
            node.run(wait_for_live=True)
        else:
            node.restart(wait_for_live=True)
        # the connect loop dials both entries at startup; give the losing handshake time to be rejected
        node.wait_number_of_blocks(3)
        node.close()

        records = _peer_records(node)
        dispositions = sorted(records[endpoint]["last_connection_disposition"] for endpoint in endpoints)
        assert dispositions == [
            "last_connection_handshaking_failed",
            "last_connection_succeeded",
        ], f"after run {run} expected one seed address connected and the other dialed and turned away: {records}"
        for endpoint in endpoints:
            assert records[endpoint]["number_of_failed_connection_attempts"] == 0, records[endpoint]
            assert records[endpoint]["number_of_successful_connection_attempts"] >= 1, records[endpoint]


def test_different_chain_rejection_still_grows_retry_backoff() -> None:
    seed = tt.InitNode()
    seed.run(arguments=["--chain-id=44"])
    (endpoint,) = _seed_endpoints(seed, "127.0.0.1")

    node = tt.ApiNode()
    node.config.p2p_seed_node.append(P2PUrl(endpoint))
    node.run(wait_for_live=False)
    seed.wait_number_of_blocks(3)
    node.close()

    record = _peer_records(node)[endpoint]
    assert record["last_connection_disposition"] == "last_connection_rejected", record
    assert record["number_of_failed_connection_attempts"] >= 1, record
