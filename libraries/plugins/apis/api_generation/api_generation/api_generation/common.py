from __future__ import annotations

from typing import Final, Literal, get_args

AvailableApis = Literal[
    "account_by_key_api",
    "account_history_api",
    "app_status_api",
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

available_apis: Final[tuple[AvailableApis, ...]] = get_args(AvailableApis)
"""All APIs, in the order of `AvailableApis`."""
APIS_WITH_LEGACY_ARGS_SERIALIZATION: Final[list[str]] = [
    "condenser_api",
    "bridge",
    "wallet_bridge_api",
]


def pascal_case(snake_case: str) -> str:
    """The name in PascalCase - e.g. `TransferToVesting` of `transfer_to_vesting`, `DatabaseApi` of `database_api`."""
    return "".join(word.capitalize() for word in snake_case.split("_"))
