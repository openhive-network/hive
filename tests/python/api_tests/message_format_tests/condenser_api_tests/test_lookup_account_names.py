from __future__ import annotations

from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_lookup_account_names(node):
    response = node.api.condenser.lookup_account_names(["initminer"], True)
    assert len(response) != 0


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_lookup_account_names_with_default_second_argument(node):
    response = node.api.condenser.lookup_account_names(["initminer"])
    assert len(response) != 0
