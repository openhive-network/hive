from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_witness_schedule(node):
    node.api.condenser.get_witness_schedule()

@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_witness_schedule_current(node):
    node.api.condenser.get_witness_schedule(False)

@run_for('testnet')
def test_get_witness_schedule_future(node):
    node.api.condenser.get_witness_schedule(True)
