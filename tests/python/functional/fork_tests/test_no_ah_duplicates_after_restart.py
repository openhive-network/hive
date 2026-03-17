from __future__ import annotations

import time

import pytest
from beekeepy.exceptions import CommunicationError, FailedToStartExecutableError

import test_tools as tt
from test_tools import complex_networks as ttcn


@pytest.mark.fork_tests_group_3
def test_no_duplicates_in_account_history_plugin_after_restart(prepare_with_many_witnesses: ttcn.NetworksBuilder) -> None:
    # TRIGGER
    # We restart one of nodes.
    networks_builder = prepare_with_many_witnesses
    api_node = networks_builder.networks[0].node("FullApiNode0")

    head_block_num = api_node.api.condenser.get_dynamic_global_properties().head_block_number
    head_block_timestamp = api_node.api.block.get_block(block_num=head_block_num).block.timestamp
    absolute_start_time = tt.Time.parse(head_block_timestamp)
    absolute_start_time -= tt.Time.seconds(5)  # Node starting and entering live mode takes some time to complete

    tt.logger.info(f"Restarting api node with time offset: {head_block_timestamp}")

    # Retry logic for flaky node restarts in CI environments
    max_retries = 3
    for attempt in range(max_retries):
        try:
            api_node.restart(time_control=tt.StartTimeControl(start_time=absolute_start_time))
            break
        except (FailedToStartExecutableError, CommunicationError, TimeoutError):
            if attempt < max_retries - 1:
                tt.logger.warning(f"Node restart failed (attempt {attempt + 1}/{max_retries}), retrying...")
                time.sleep(1)
            else:
                raise

    # VERIFY
    # Expected behaviour is that restarted node will enter live sync
    # We check there are no duplicates in account_history_api after such scenario (issue #117).
    tt.logger.info("Assert there are no duplicates in account_history.get_ops_in_block after node restart...")
    ttcn.assert_no_duplicates(api_node)
