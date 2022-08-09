from typing import Dict, List

import pytest

import test_tools as tt

from ..local_tools import prepare_witnesses
from .local_tools import connect_sub_networks
from test_tools.__private.user_handles import WitnessNodeHandle

def prepared_sub_networks(sub_networks_sizes : list) -> Dict:

    assert len(sub_networks_sizes) > 0, "At least 1 sub-network is required"
    cnt               = 0
    all_witness_names = []
    sub_networks      = []
    init_node         = None

    for sub_networks_size in sub_networks_sizes:
        tt.logger.info(f'Preparing sub-network nr: {cnt} that consists of {sub_networks_size} nodes')

        witness_names = [f'witness-{cnt}-{i}' for i in range(sub_networks_size)]
        all_witness_names += witness_names

        sub_network = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
        if cnt == 0:
            init_node = tt.InitNode(network = sub_network)
        sub_networks.append(sub_network)
        tt.WitnessNode(witnesses = witness_names, network = sub_network)
        tt.ApiNode(network = sub_network)

        cnt += 1

    # Run
    connect_sub_networks(sub_networks)

    tt.logger.info('Running networks, waiting for live...')

    for sub_network in sub_networks:
        assert sub_network is not None
        sub_network.run()

    init_wallet = prepare_witnesses(init_node, all_witness_names)
    return sub_networks, all_witness_names, init_wallet

@pytest.fixture(scope="package")
def prepared_sub_networks_10_11() -> Dict:
    yield { 'sub-networks-data': prepared_sub_networks([10, 11]) }

@pytest.fixture(scope="package")
def prepared_sub_networks_3_18() -> Dict:
    yield { 'sub-networks-data': prepared_sub_networks([3, 18]) }

@pytest.fixture(scope="package")
def prepared_sub_networks_6_17() -> Dict:
    yield { 'sub-networks-data': prepared_sub_networks([6, 17]) }

@pytest.fixture(scope="package")
def prepared_sub_networks_1_3_17() -> Dict:
    yield { 'sub-networks-data': prepared_sub_networks([1, 3, 17]) }

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

    for node in [*alpha_net.nodes, *beta_net.nodes]:
        node.config.webserver_thread_pool_size="2"
        node.config.blockchain_thread_pool_size=2
        node.config.log_logger = '{"name":"default","level":"debug","appender":"stderr,p2p"} '\
                                  '{"name":"user","level":"debug","appender":"stderr,p2p"} '\
                                  '{"name":"chainlock","level":"debug","appender":"p2p"} '\
                                  '{"name":"sync","level":"debug","appender":"p2p"} '\
                                  '{"name":"p2p","level":"debug","appender":"p2p"}'

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
