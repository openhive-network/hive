from __future__ import annotations

from typing import Literal, Final


AvailableApis = Literal[
    "account_by_key_api",
    "account_history_api",
    "block_api",
    "bridge",
    "condenser_api",
    "database_api",
    "debug_node_api",
    "follow_api",
    "hive",
    "market_history_api",
    "network_broadcast_api",
    "rc_api",
    "reputation_api",
    "rewards_api",
    "search-api",
    "tags_api",
    "transaction_status_api",
    "wallet_bridge_api",
    "witness_api",
]

available_apis: Final[list[AvailableApis]] = [
    "account_by_key_api",
    "account_history_api",
    "block_api",
    "bridge",
    "condenser_api",
    "database_api",
    "debug_node_api",
    "follow_api",
    "hive",
    "market_history_api",
    "network_broadcast_api",
    "rc_api",
    "reputation_api",
    "rewards_api",
    "search-api",
    "tags_api",
    "transaction_status_api",
    "wallet_bridge_api",
    "witness_api",
]
