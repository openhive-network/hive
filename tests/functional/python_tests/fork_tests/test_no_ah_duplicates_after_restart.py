import test_tools as tt

from .local_tools import assert_no_duplicates


def test_no_duplicates_in_account_history_plugin_after_restart(prepared_networks):
    # TRIGGER
    # We restart one of nodes.
    api_node = prepared_networks['Alpha'].node('ApiNode0')
    tt.logger.info('Restarting api node...')
    api_node.restart()

    # VERIFY
    # Expected behaviour is that restarted node will enter live sync
    # We check there are no duplicates in account_history_api after such scenario (issue #117).
    tt.logger.info('Assert there are no duplicates in account_history.get_ops_in_block after node restart...')
    assert_no_duplicates(api_node)
