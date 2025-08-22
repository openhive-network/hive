from __future__ import annotations

from typing import Literal, Final

AvailableApis = Literal["block_api", "database_api", "rc_api", "network_broadcast_api", "account_by_key_api", "account_history_api", "bridge"]
available_apis: Final[list[AvailableApis]] = [
    "block_api",
    "database_api",
    "rc_api",
    "network_broadcast_api",
    "account_by_key_api",
    "account_history_api",
    "bridge",
]
