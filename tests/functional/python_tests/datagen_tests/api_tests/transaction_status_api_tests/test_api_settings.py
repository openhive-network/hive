from pathlib import Path

import pytest

from hive_local_tools.functional.python.datagen.api.transaction_status_api import read_transaction_ids,\
    verify_transaction_status_in_block_range

__PATTERNS_DIRECTORY = Path(__file__).with_name('block_log')

"""
Real transaction_status_block_depth = transaction_status_block_depth + HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL 
Real transaction_status_track_after_block = transaction_status_track_after_block - HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL 
"""


@pytest.mark.transaction_status_track_after_block('1200')
def test_transaction_status_track_after_0_block(replayed_node):
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 101, transactions, 'within_irreversible_block')


@pytest.mark.transaction_status_track_after_block('1300')
def test_transaction_status_track_after_100_block(replayed_node):
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 101, transactions, 'unknown')


@pytest.mark.transaction_status_block_depth('100')
def test_transaction_status_1300_block_depth(replayed_node):
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 191, transactions, 'unknown')


@pytest.mark.transaction_status_block_depth('300')
def test_transaction_status_1500_block_depth(replayed_node):
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 191, transactions, 'within_irreversible_block')


@pytest.mark.transaction_status_block_depth('100')
def test_transaction_status_rebuild_state(replayed_node):
    transactions = read_transaction_ids(__PATTERNS_DIRECTORY)
    verify_transaction_status_in_block_range(replayed_node, 0, 191, transactions, 'unknown')

    replayed_node.close()
    replayed_node.config.transaction_status_block_depth = '300'
    replayed_node.run(wait_for_live=False, arguments=['--transaction-status-rebuild-state'])

    verify_transaction_status_in_block_range(replayed_node, 0, 191, transactions, 'within_irreversible_block')
