from test_tools import logger

from .local_tools import assert_no_duplicates


def test_no_duplicates_in_account_history_plugin_after_restart(world_with_witnesses):
    world = world_with_witnesses

    # TRIGGER
    # We restart one of nodes.
    api_node = world.network('Alpha').node('ApiNode0')
    logger.info("Restarting api node...")
    api_node.restart()

    # VERIFY
    # Expected behavour is that restarted node will enter live sync
    # We check there are no duplicates in account_history_api after such scenario (issue #117).
    logger.info("Assert there are no duplicates in account_history.get_ops_in_block after node restart...")
    assert_no_duplicates(api_node)
