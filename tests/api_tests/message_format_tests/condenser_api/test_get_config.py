from ..local_tools import run_for


@run_for('testnet')
def test_get_config(prepared_node):
    prepared_node.api.condenser.get_config()
