from typing import Dict

import pytest

import test_tools as tt

from ..local_tools import prepare_witnesses
from test_tools.__private.user_handles import WitnessNodeHandle

@pytest.fixture(scope="package")
def prepared_networks() -> Dict:
    """
    Fixture consists of 1 init node, 8 witness nodes and 2 api nodes.
    After fixture creation there are 21 active witnesses, and last irreversible block
    is behind head block like in real network.
    """

    tt.logger.info('Preparing fixture prepared_networks')
    alpha_witness_names = [f'witness{i}-alpha' for i in range(10)]
    beta_witness_names = [f'witness{i}-beta' for i in range(10)]
    all_witness_names = alpha_witness_names + beta_witness_names

    # Create first network
    alpha_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    init_node = tt.InitNode(network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(0, 3)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(3, 6)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(6, 8)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(8, 10)], network=alpha_net)
    alpha_api_node = tt.ApiNode(network=alpha_net)

    # Create second network
    beta_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(0, 3)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(3, 6)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(6, 8)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(8, 10)], network=beta_net)
    tt.ApiNode(network=beta_net)

    # Run
    alpha_net.connect_with(beta_net)


    tt.logger.info('Running networks, waiting for live...')
    alpha_net.run()
    beta_net.run()

    # tt.logger.info('Disabling fast confirm on all witnesses')
    # for node in alpha_net.nodes + beta_net.nodes:
    #     if isinstance(node, WitnessNodeHandle):
    #         tt.logger.error('Disabling fast confirm')
    #         node.disable_fast_confirm()

    prepare_witnesses(init_node, all_witness_names)


    yield {
        'Alpha': alpha_net,
        'Beta': beta_net,
    }
