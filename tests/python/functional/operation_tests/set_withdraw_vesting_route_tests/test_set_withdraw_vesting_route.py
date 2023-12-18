"""Scenarios description: https://gitlab.syncad.com/hive/hive/-/issues/539"""
from __future__ import annotations

import pytest

import test_tools as tt

from hive_local_tools.functional.python.operation import get_virtual_operations


@pytest.fixture()
def node():
    node = tt.InitNode()
    node.run()
    return node

@pytest.fixture()
def wallet(node):
    wallet = tt.Wallet(attach_to=node)
    return wallet


def test_set_withdraw_vesting_route(node, wallet):
    """
    There is the Power Down in progress created by user A. It is after the first withdrawal.
    """
    wallet.create_account("alice", vests=tt.Asset.Test(1_000))
    wallet.api.withdraw_vesting("alice", tt.Asset.Vest(100))

    hardfork_schedule = [
        {"hardfork": 2, "block_num": 0},
        {"hardfork": 20, "block_num": 20},
        {"hardfork": 27, "block_num": 380},
    ]
    directory = tt.context.get_current_directory()

    cs = tt.ChainSpec(path=directory, hardfork_schedule=hardfork_schedule)
    cs.save_to_json()

    print()
