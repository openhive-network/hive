import test_tools as tt

from .local_tools import make_fork, reconnect_networks


def test_fast_confirm_not_decreasing_head(prepared_networks_witnesses_18_2):
    tt.logger.info(f'Start test_fast_confirm_not_decreasing_head')

    # GIVEN
    networks = prepared_networks_witnesses_18_2
    alpha_node = networks['Alpha'].nodes[0]
    beta_node = networks['Beta'].nodes[0]

    # WHEN
    fork_block = make_fork(networks, blocks_in_fork=0)

    alpha_node.wait_for_block_with_number(fork_block+1)
    alpha_node.api.debug_node.debug_disable_block_production()

    beta_node.wait_for_block_with_number(fork_block+3)
    beta_node.api.debug_node.debug_disable_block_production()

    # THEN
    # There are forks C and D'. Beta network should stay on D' because fork switching doesn't happen for shorter fork
    # A-----------B---C
    # \--B'--C'--D'
    reconnect_networks(networks)

    alpha_node.api.debug_node.debug_enable_block_production()
    alpha_node.wait_number_of_blocks(1)
    alpha_node.api.debug_node.debug_disable_block_production()

    dgpo = beta_node.api.database.get_dynamic_global_properties()
    tt.logger.info(f'beta node dgpo {dgpo}')

    assert beta_node.get_number_of_forks() == 0, 'switching forks detected before it was expected'
