import test_tools as tt

from .local_tools import make_fork, wait_for_back_from_fork, reconnect_networks


def test_fast_confirm_choosing_fork(prepared_networks_witnesses_18_2):
    tt.logger.info(f'Start test_fast_confirm_choosing_fork')

    # GIVEN
    networks = prepared_networks_witnesses_18_2
    alpha_node = networks['Alpha'].nodes[0]
    beta_node = networks['Beta'].nodes[0]

    # WHEN
    fork_block = make_fork(networks, blocks_in_fork=0)

    alpha_node.wait_for_block_with_number(fork_block+1)
    alpha_node.api.debug_node.debug_disable_block_production()

    beta_node.wait_for_block_with_number(fork_block+2)
    beta_node.api.debug_node.debug_disable_block_production()

    # THEN
    # There are forks B and C'. Beta network should stay on fork C'
    # A--------------B
    # \--B'--C'
    reconnect_networks(networks)

    dgpo = beta_node.api.database.get_dynamic_global_properties()
    tt.logger.info(f'beta node dgpo {dgpo}')

    assert beta_node.get_number_of_forks() == 0, 'switching forks detected before it was expected'

    # There are forks C and C', C is confirmed by fast confirmation. Beta network should switch to C
    # A--------------B---C
    # \--B'--C'
    alpha_node.api.debug_node.debug_enable_block_production()
    alpha_node.wait_number_of_blocks(1)
    alpha_node.api.debug_node.debug_disable_block_production()
    wait_for_back_from_fork(beta_node)

    dgpo = beta_node.api.database.get_dynamic_global_properties()
    tt.logger.info(f'beta node dgpo {dgpo}')

    assert beta_node.get_number_of_forks() == 1, 'switching forks did not occur'
    assert dgpo["head_block_number"] == dgpo["last_irreversible_block_num"], 'head block should be confirmed by fast confirmations'
