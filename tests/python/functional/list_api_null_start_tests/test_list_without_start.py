"""Tests that all list_* database API calls work correctly when 'start' is omitted (null/nullopt).

After commit 2ed2cf50, the 'start' parameter is optional in every list database API function.
When omitted, iterate_results iterates from the beginning (ascending) or end (descending) of the index.
These tests verify that every (method, order) combination returns a valid response without crashing.
"""

from __future__ import annotations

import pytest
import requests

import test_tools as tt

# Each tuple: (method_name, params_without_start, expected_result_key, min_start)
# params always has "limit" and "order" (where applicable), but never "start".
# min_start is the explicit minimum start value for comparison; None means the equality check is skipped.
EPOCH = "1970-01-01T00:00:00"
LIST_API_CASES = [
    # list_witnesses
    ("list_witnesses", {"limit": 100, "order": "by_name"}, "witnesses", ""),
    ("list_witnesses", {"limit": 100, "order": "by_vote_name"}, "witnesses", [0, ""]),
    ("list_witnesses", {"limit": 100, "order": "by_schedule_time"}, "witnesses", None),
    # list_witness_votes
    ("list_witness_votes", {"limit": 100, "order": "by_account_witness"}, "votes", ["", ""]),
    ("list_witness_votes", {"limit": 100, "order": "by_witness_account"}, "votes", ["", ""]),
    # list_accounts
    ("list_accounts", {"limit": 100, "order": "by_name"}, "accounts", ""),
    ("list_accounts", {"limit": 100, "order": "by_proxy"}, "accounts", ["", ""]),
    ("list_accounts", {"limit": 100, "order": "by_next_vesting_withdrawal"}, "accounts", [EPOCH, ""]),
    # list_owner_histories (no order param)
    ("list_owner_histories", {"limit": 100}, "owner_auths", ["", EPOCH]),
    # list_account_recovery_requests
    ("list_account_recovery_requests", {"limit": 100, "order": "by_account"}, "requests", ""),
    ("list_account_recovery_requests", {"limit": 100, "order": "by_expiration"}, "requests", [EPOCH, ""]),
    # list_change_recovery_account_requests
    ("list_change_recovery_account_requests", {"limit": 100, "order": "by_account"}, "requests", ""),
    ("list_change_recovery_account_requests", {"limit": 100, "order": "by_effective_date"}, "requests", [EPOCH, ""]),
    # list_escrows
    ("list_escrows", {"limit": 100, "order": "by_from_id"}, "escrows", ["", 0]),
    ("list_escrows", {"limit": 100, "order": "by_ratification_deadline"}, "escrows", [False, EPOCH, 0]),
    # list_withdraw_vesting_routes
    ("list_withdraw_vesting_routes", {"limit": 100, "order": "by_withdraw_route"}, "routes", ["", ""]),
    ("list_withdraw_vesting_routes", {"limit": 100, "order": "by_destination"}, "routes", ["", 0]),
    # list_savings_withdrawals
    ("list_savings_withdrawals", {"limit": 100, "order": "by_from_id"}, "withdrawals", ["", 0]),
    ("list_savings_withdrawals", {"limit": 100, "order": "by_complete_from_id"}, "withdrawals", [EPOCH, "", 0]),
    ("list_savings_withdrawals", {"limit": 100, "order": "by_to_complete"}, "withdrawals", ["", EPOCH, 0]),
    # list_vesting_delegations
    ("list_vesting_delegations", {"limit": 100, "order": "by_delegation"}, "delegations", None),
    # list_vesting_delegation_expirations
    ("list_vesting_delegation_expirations", {"limit": 100, "order": "by_expiration"}, "delegations", [EPOCH, 0]),
    (
        "list_vesting_delegation_expirations",
        {"limit": 100, "order": "by_account_expiration"},
        "delegations",
        ["", EPOCH, 0],
    ),
    # list_hbd_conversion_requests
    ("list_hbd_conversion_requests", {"limit": 100, "order": "by_conversion_date"}, "requests", [EPOCH, 0]),
    ("list_hbd_conversion_requests", {"limit": 100, "order": "by_account"}, "requests", ["", 0]),
    # list_collateralized_conversion_requests
    (
        "list_collateralized_conversion_requests",
        {"limit": 100, "order": "by_conversion_date"},
        "requests",
        [EPOCH, 0],
    ),
    ("list_collateralized_conversion_requests", {"limit": 100, "order": "by_account"}, "requests", ["", 0]),
    # list_decline_voting_rights_requests
    ("list_decline_voting_rights_requests", {"limit": 100, "order": "by_account"}, "requests", ""),
    ("list_decline_voting_rights_requests", {"limit": 100, "order": "by_effective_date"}, "requests", [EPOCH, ""]),
    # list_limit_orders
    ("list_limit_orders", {"limit": 100, "order": "by_price"}, "orders", None),
    ("list_limit_orders", {"limit": 100, "order": "by_account"}, "orders", ["", 0]),
    # list_proposals
    (
        "list_proposals",
        {"limit": 100, "order": "by_creator", "order_direction": "ascending", "status": "all"},
        "proposals",
        [""],
    ),
    (
        "list_proposals",
        {"limit": 100, "order": "by_start_date", "order_direction": "ascending", "status": "all"},
        "proposals",
        [""],
    ),
    (
        "list_proposals",
        {"limit": 100, "order": "by_end_date", "order_direction": "ascending", "status": "all"},
        "proposals",
        [""],
    ),
    (
        "list_proposals",
        {"limit": 100, "order": "by_total_votes", "order_direction": "ascending", "status": "all"},
        "proposals",
        [0],
    ),
    # list_proposal_votes
    (
        "list_proposal_votes",
        {"limit": 100, "order": "by_voter_proposal", "order_direction": "ascending", "status": "all"},
        "proposal_votes",
        ["", 0],
    ),
    (
        "list_proposal_votes",
        {"limit": 100, "order": "by_proposal_voter", "order_direction": "ascending", "status": "all"},
        "proposal_votes",
        [0, ""],
    ),
]


def call_api(node: tt.InitNode, method: str, params: dict) -> dict:
    """Send a raw JSON-RPC request to database_api.<method>."""
    url = node.http_endpoint.as_string()
    payload = {
        "jsonrpc": "2.0",
        "method": f"database_api.{method}",
        "params": params,
        "id": 1,
    }
    response = requests.post(url, json=payload, timeout=30)
    response.raise_for_status()
    return response.json()


@pytest.mark.parametrize(
    ("method", "params", "expected_key", "min_start"),
    LIST_API_CASES,
    ids=[f"{m}-{p.get('order', 'default')}" for m, p, _, _ms in LIST_API_CASES],
)
def test_list_without_start(
    prepared_node: tt.InitNode, method: str, params: dict, expected_key: str, min_start: object
) -> None:
    """Verify that calling database_api.<method> without 'start' returns the same data as with explicit min start."""
    response = call_api(prepared_node, method, params)

    assert "error" not in response, f"API returned error: {response.get('error')}"
    assert "result" in response, f"API response missing 'result': {response}"

    result = response["result"]
    assert expected_key in result, f"Result missing expected key '{expected_key}': {list(result.keys())}"
    assert isinstance(result[expected_key], list), (
        f"Expected list for '{expected_key}', got {type(result[expected_key])}"
    )

    if min_start is not None:
        params_with_start = {**params, "start": min_start}
        response_with_start = call_api(prepared_node, method, params_with_start)
        assert "error" not in response_with_start, (
            f"API returned error with explicit start: {response_with_start.get('error')}"
        )
        assert response_with_start["result"][expected_key] == result[expected_key], (
            f"Results differ between no-start and explicit min-start for {method}"
        )
