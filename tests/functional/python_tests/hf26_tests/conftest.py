import pytest

import test_tools as tt

from .local_tools import run_with_faketime, prepare_environment, prepare_environment_with_2_sub_networks

@pytest.fixture
def node_hf25() -> tt.InitNode:
    node = tt.InitNode()
    run_with_faketime(node, '2022-10-10T10:10:10')
    return node


@pytest.fixture
def node_hf26() -> tt.InitNode:
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture
def wallet_hf25(node_hf25) -> tt.Wallet:
    return tt.Wallet(attach_to=node_hf25)


@pytest.fixture
def wallet_hf26(node_hf26) -> tt.Wallet:
    return tt.Wallet(attach_to=node_hf26)


@pytest.fixture
def network_before_hf26():
    tt.logger.info('Preparing fixture network_before_hf26')

    alpha_net = prepare_environment('2023-11-08T07:41:40')

    yield {
        'alpha': alpha_net
    }


@pytest.fixture
def network_after_hf26():
    tt.logger.info('Preparing fixture network_after_hf26')

    alpha_net = prepare_environment('2022-06-01T07:41:41')

    yield {
        'alpha': alpha_net
    }


@pytest.fixture
def network_after_hf26_without_majority():
    tt.logger.info('Preparing fixture network_after_hf26_without_majority')

    alpha_net, beta_net = prepare_environment_with_2_sub_networks('2032-06-20T09:45:38', '2022-06-01T07:41:41')

    yield {
        'alpha': alpha_net,
        'beta': beta_net,
    }
