from ..local_tools import run_for


@run_for('testnet')
def test_get_rc_stats(prepared_node, should_prepare):
    a = prepared_node.api.rc.get_rc_stats()
    print()
