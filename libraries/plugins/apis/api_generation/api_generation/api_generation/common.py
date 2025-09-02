from __future__ import annotations

from typing import Literal, Final

AvailableApis = Literal[
"condenser_api",
"witness_api",
"wallet_bridge_api",
"transaction_status_api",
"bridge",
"block_api",
"database_api",
"rc_api",
"network_broadcast_api",
"account_by_key_api",
"account_history_api",
"tags_api",
"rewards_api",
"reputation_api",
"market_history_api",
"follow_api",
"debug_node_api",
]

available_apis: Final[list[AvailableApis]] = [
    "condenser_api",
    "witness_api",
    "wallet_bridge_api",
    "transaction_status_api",
    "block_api",
    "database_api",
    "rc_api",
    "network_broadcast_api",
    "account_by_key_api",
    "account_history_api",
    "bridge",
    "tags_api",
    "rewards_api",
    "reputation_api",
    "market_history_api",
    "follow_api",
    "debug_node_api",
]
