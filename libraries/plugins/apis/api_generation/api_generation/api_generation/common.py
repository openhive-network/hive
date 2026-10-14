from __future__ import annotations

from typing import Final, Literal

AvailableApis = Literal[
    "account_by_key_api",
    "account_history_api",
    "app_status_api",
    "beekeeper_api",
    "block_api",
    "bridge",
    "condenser_api",
    "database_api",
    "debug_node_api",
    "follow_api",
    "jsonrpc",
    "hive",
    "market_history_api",
    "network_broadcast_api",
    "network_node_api",
    "rc_api",
    "reputation_api",
    "search_api",
    "tags_api",
    "transaction_status_api",
    "wallet_bridge_api",
]

available_apis: Final[list[AvailableApis]] = [
    "account_by_key_api",
    "account_history_api",
    "app_status_api",
    "beekeeper_api",
    "block_api",
    "bridge",
    "condenser_api",
    "database_api",
    "debug_node_api",
    "follow_api",
    "jsonrpc",
    "hive",
    "market_history_api",
    "network_broadcast_api",
    "network_node_api",
    "rc_api",
    "reputation_api",
    "search_api",
    "tags_api",
    "transaction_status_api",
    "wallet_bridge_api",
]
APIS_WITH_LEGACY_ARGS_SERIALIZATION: Final[list[str]] = [
    "condenser_api",
    "bridge",
    "wallet_bridge_api",
]
