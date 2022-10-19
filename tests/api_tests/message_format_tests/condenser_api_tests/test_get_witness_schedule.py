from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_witness_schedule(prepared_node):
    prepared_node.api.condenser.get_witness_schedule()

@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_witness_schedule_current(prepared_node):
    prepared_node.api.condenser.get_witness_schedule(False)

@run_for('testnet')
def test_get_witness_schedule_future(prepared_node):
    prepared_node.api.condenser.get_witness_schedule(True)
