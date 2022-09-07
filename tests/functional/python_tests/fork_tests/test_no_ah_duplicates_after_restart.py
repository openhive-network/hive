import test_tools as tt

from .local_tools import assert_no_duplicates, get_time_offset_from_iso_8601


def test_no_duplicates_in_account_history_plugin_after_restart(prepared_networks):
    # TRIGGER
    # We restart one of nodes.
    api_node = prepared_networks['Alpha'].node('ApiNode0')

    head_block_num = api_node.api.condenser.get_dynamic_global_properties()['head_block_number']
    head_block_timestamp = api_node.api.block.get_block(block_num=head_block_num)['block']['timestamp']
    time_offset = get_time_offset_from_iso_8601(head_block_timestamp)

    tt.logger.info(f'Restarting api node with time offset: {head_block_timestamp}')
    api_node.restart(time_offset=time_offset)

    # VERIFY
    # Expected behaviour is that restarted node will enter live sync
    # We check there are no duplicates in account_history_api after such scenario (issue #117).
    tt.logger.info('Assert there are no duplicates in account_history.get_ops_in_block after node restart...')
    assert_no_duplicates(api_node)
