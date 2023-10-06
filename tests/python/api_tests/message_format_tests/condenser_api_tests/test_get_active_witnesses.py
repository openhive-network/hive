from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_active_witnesses(node):
    node.api.condenser.get_active_witnesses()


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_active_witnesses_current(node):
    node.api.condenser.get_active_witnesses(False)


@run_for("testnet")
def test_get_active_witnesses_future(node):
    node.api.condenser.get_active_witnesses(True)
