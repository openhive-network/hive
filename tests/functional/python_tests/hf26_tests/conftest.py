
from typing import Dict, List, Optional

import pytest

import test_tools as tt

def prepare_witnesses( init_node, api_node, all_witness_names : List[str] ):

    tt.logger.info('Attaching wallets...')
    wallet = tt.Wallet(attach_to=api_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    tt.logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
    init_node.wait_for_block_with_number(43)

    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.create_account('initminer', name, '')
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.update_witness(
                name, "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    tt.logger.info('Wait 21 blocks to schedule newly created witnesses')
    init_node.wait_number_of_blocks(21)

    tt.logger.info("Witness state after voting")
    response = api_node.api.database.list_witnesses(start=0, limit=100, order='by_name')
    active_witnesses = response["witnesses"]
    active_witnesses_names = [witness["owner"] for witness in active_witnesses]
    tt.logger.info(active_witnesses_names)
    assert len(active_witnesses_names) == 21

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info('Wait 21 blocks (when every witness sign at least one block)')
    init_node.wait_number_of_blocks(21)

    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    tt.logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

    assert irreversible + 10 < head

def prepare_network(number: int, network_name : str, allow_create_init_node : bool, allow_create_api_node : bool):
    tt.logger.info(f'Running network, waiting for live...')

    witness_names = [f'wit{i}-{network_name}' for i in range(number)]

    network = tt.Network()

    init_node = None
    if allow_create_init_node:
        init_node = tt.InitNode(network=network)

    tt.WitnessNode(witnesses = witness_names, network=network)

    api_node = None
    if allow_create_api_node:
        api_node = tt.ApiNode(network=network)

    return witness_names, network, init_node, api_node

def prepare_environment(environment_variables: Optional[Dict] = None):
    witnesses_number        = 20
    allow_create_init_node  = True
    allow_create_api_node   = True
    all_witness_names, alpha_net, init_node, api_node = prepare_network(witnesses_number, 'alpha', allow_create_init_node, allow_create_api_node)

    # Run
    tt.logger.info('Running networks, waiting for live...')
    alpha_net.run(environment_variables)

    prepare_witnesses(init_node, api_node, all_witness_names)

    return alpha_net

def prepare_environment_with_2_sub_networks(environment_variables_alpha: Optional[Dict] = None, environment_variables_beta: Optional[Dict] = None):
    #Because HIVE_HARDFORK_REQUIRED_WITNESSES = 17 // 17 of the 21 dpos witnesses (20 elected and 1 virtual time) required for hardfork
    witnesses_number        = 8
    allow_create_init_node  = True
    allow_create_api_node   = True
    witness_names_alpha, alpha_net, init_node, api_node = prepare_network(witnesses_number, 'alpha', allow_create_init_node, allow_create_api_node)

    witnesses_number        = 12
    allow_create_init_node  = False
    allow_create_api_node   = False
    witness_names_beta, beta_net, init_node2, api_node2 = prepare_network(witnesses_number, 'beta', allow_create_init_node, allow_create_api_node)

    all_witness_names = witness_names_alpha + witness_names_beta

    alpha_net.connect_with(beta_net)

    # Run
    tt.logger.info('Running networks, waiting for live...')

    #June 20, 2032 9:45:38 AM
    alpha_net.run(environment_variables_alpha)

    #June 1, 2022 7:41:41 AM
    beta_net.run(environment_variables_beta)

    prepare_witnesses(init_node, api_node, all_witness_names)

    return alpha_net, beta_net

@pytest.fixture(scope="package")
def network_before_hf26():
    tt.logger.info('Preparing fixture network_before_hf26')

    #November 8, 2023 7:41:40 AM
    alpha_net = prepare_environment(environment_variables={"HIVE_HF26_TIME": "1699429300"})

    yield {
        'alpha': alpha_net
    }

@pytest.fixture(scope="package")
def network_after_hf26():
    tt.logger.info('Preparing fixture network_after_hf26')

    #June 1, 2022 7:41:41 AM
    alpha_net = prepare_environment(environment_variables={"HIVE_HF26_TIME": "1654069301"})

    yield {
        'alpha': alpha_net
    }

@pytest.fixture(scope="package")
def network_after_hf26_without_majority():
    tt.logger.info('Preparing fixture network_after_hf26_without_majority')

    #1971337538 = June 20, 2032 9:45:38 AM
    #1654069301 = June 1, 2022 7:41:41 AM
    alpha_net, beta_net = prepare_environment_with_2_sub_networks(environment_variables_alpha={"HIVE_HF26_TIME": "1971337538"}, environment_variables_beta={"HIVE_HF26_TIME": "1654069301"})

    yield {
        'alpha': alpha_net,
        'beta': beta_net,
    }

