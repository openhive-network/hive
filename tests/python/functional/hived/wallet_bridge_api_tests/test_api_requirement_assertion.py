from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ApiNotFoundError, ErrorInResponseError

if TYPE_CHECKING:
    import test_tools as tt


@pytest.mark.parametrize(
    "wallet_bridge_api_command",
    [
        "get_version",
        "get_chain_properties",
        "get_current_median_history_price",
        "get_hardfork_version",
        "get_feed_history",
        "get_dynamic_global_properties",
    ],
)
@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_no_api_is_missing(node: tt.InitNode, wallet_bridge_api_command: str) -> None:
    getattr(node.api.wallet_bridge, wallet_bridge_api_command)()


@pytest.mark.parametrize(
    "wallet_bridge_api_command",
    [
        "get_witness_schedule",
        "get_active_witnesses",
        "get_withdraw_routes",
        "get_accounts",
        "list_witnesses",
        "get_witness",
        "get_conversion_requests",
        "get_collateralized_conversion_requests",
        "get_owner_history",
        "list_proposals",
        "find_proposals",
        "list_proposal_votes",
    ],
)
@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_reporting_exception_when_database_api_is_missing(node: tt.InitNode, wallet_bridge_api_command: str) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert "Assert Exception:_database_api: database_api_plugin not enabled." not in exception.value.error


@pytest.mark.parametrize(
    "wallet_bridge_api_command",
    [
        "list_rc_accounts",
        "list_rc_direct_delegations",
    ],
)
@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_reporting_exception_when_rc_api_is_missing(node: tt.InitNode, wallet_bridge_api_command: str) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert "Assert Exception:_rc_api: rc_api_plugin not enabled." not in exception.value.error


@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_reporting_exception_when_block_api_is_missing(node: tt.InitNode) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        node.api.wallet_bridge.get_block()

    assert "Assert Exception:_block_api: block_api_plugin not enabled." not in exception.value.error


@pytest.mark.parametrize(
    "wallet_bridge_api_command",
    [
        "get_ops_in_block",
        "get_transaction",
        "get_account_history",
    ],
)
@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_reporting_exception_when_account_history_api_is_missing(
    node: tt.InitNode, wallet_bridge_api_command: str
) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    assert "Assert Exception:_account_history_api: account_history_api_plugin not enabled." in exception.value.error


@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_reporting_exception_when_account_by_key_api_is_missing(node: tt.InitNode) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        node.api.wallet_bridge.list_my_accounts()

    assert "Assert Exception:_account_by_key_api: account_by_key_api_plugin not enabled." not in exception.value.error


@pytest.mark.enabled_plugins("witness", "wallet_bridge_api")
def test_reporting_exception_when_market_history_api_is_missing(node: tt.InitNode) -> None:
    with pytest.raises(ErrorInResponseError) as exception:
        node.api.wallet_bridge.get_order_book()

    assert "Assert Exception:_market_history_api: market_history_api_plugin not enabled." not in exception.value.error


@pytest.mark.enabled_plugins("witness")
def test_reporting_exception_when_wallet_bridge_api_is_missing(node: tt.InitNode) -> None:
    with pytest.raises(ApiNotFoundError) as exception:
        node.api.wallet_bridge.get_version()

    assert "Could not find API wallet_bridge_api" in str(exception.value.response)
