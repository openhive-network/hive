from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

import pytest

from hive_local_tools.functional.python.datagen.api.transaction_status_api import (
    read_transaction_ids,
    verify_transaction_status_in_block_range,
)

if TYPE_CHECKING:
    import test_tools as tt

__PATTERNS_DIRECTORY = Path(__file__).with_name("block_log")

"""
Block log size is 1500 blocks
Real transaction_status_block_depth = transaction_status_block_depth + 1200
  (transaction_status_block_depth + HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL)
Real default transaction_status_block_depth = 64000 + 1200
  (64000 + HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL)
"""


@pytest.mark.transaction_status_block_depth("64000")
def test_transaction_status_default_block_depth(replayed_node: tt.ApiNode) -> None:
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 101, transactions, "within_irreversible_block")


@pytest.mark.transaction_status_block_depth("100")
def test_transaction_status_1300_block_depth(replayed_node: tt.ApiNode) -> None:
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 191, transactions, "unknown")


@pytest.mark.transaction_status_block_depth("300")
def test_transaction_status_1500_block_depth(replayed_node: tt.ApiNode) -> None:
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 191, transactions, "within_irreversible_block")
