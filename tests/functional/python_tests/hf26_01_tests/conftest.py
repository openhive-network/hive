import pytest

from typing import Dict, List, Optional

from test_tools import Account, Asset, constants, logger, Wallet, World
from test_tools.private.scope import context
from test_tools.private.node import Node

def prepare_witnesses( init_node, api_node: Node, all_witness_names : List[str] ):

    logger.info('Attaching wallets...')
    wallet = Wallet(attach_to=api_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
    init_node.wait_for_block_with_number(43)

    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.create_account('initminer', name, '')
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.update_witness(
                name, "https://" + name,
                Account(name).public_key,
                {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    logger.info('Wait 21 blocks to schedule newly created witnesses')
    init_node.wait_number_of_blocks(21)

    logger.info("Witness state after voting")
    response = api_node.api.database.list_witnesses(start=0, limit=100, order='by_name')
    active_witnesses = response["witnesses"]
    active_witnesses_names = [witness["owner"] for witness in active_witnesses]
    logger.info(active_witnesses_names)
    assert len(active_witnesses_names) == 21

    # Reason of this wait is to enable moving forward of irreversible block
    logger.info('Wait 21 blocks (when every witness sign at least one block)')
    init_node.wait_number_of_blocks(21)

    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

    assert irreversible + 10 < head

def prepare_network(world : World, name : str, number: int, allow_create_init_node : bool, allow_create_api_node : bool):
    logger.info(f'Running {name} network, waiting for live...')

    witness_names = [f'wit{i}-{name}' for i in range(number)]

    network = world.create_network(name)

    init_node = None
    if allow_create_init_node:
        init_node = network.create_init_node()

    network.create_witness_node(witnesses = witness_names)

    api_node = None
    if allow_create_api_node:
        api_node = network.create_api_node()

    return witness_names, network, init_node, api_node

def prepare_world(world : World, network_number :int, environment_variables: Optional[Dict] = None):
    world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)

    witnesses_number        = 20
    allow_create_init_node  = True
    allow_create_api_node   = True
    all_witness_names, alpha_net, init_node, api_node = prepare_network(world, f'alpha-{network_number}', witnesses_number, allow_create_init_node, allow_create_api_node)

    # Run
    logger.info('Running networks, waiting for live...')
    wait_for_live=True
    alpha_net.run(wait_for_live, environment_variables)

    prepare_witnesses(init_node, api_node, all_witness_names)

def prepare_world_with_2_sub_networks(world : World, network_number_alpha :int, network_number_beta :int, environment_variables_alpha: Optional[Dict] = None, environment_variables_beta: Optional[Dict] = None):
    #Because HIVE_HARDFORK_REQUIRED_WITNESSES = 17 // 17 of the 21 dpos witnesses (20 elected and 1 virtual time) required for hardfork
    witnesses_number        = 8
    allow_create_init_node  = True
    allow_create_api_node   = True
    witness_names_alpha, alpha_net, init_node, api_node = prepare_network(world, f'alpha-{network_number_alpha}', witnesses_number, allow_create_init_node, allow_create_api_node)

    witnesses_number        = 12
    allow_create_init_node  = False
    allow_create_api_node   = False
    witness_names_beta, beta_net, init_node2, api_node2 = prepare_network(world, f'beta-{network_number_beta}', witnesses_number, allow_create_init_node, allow_create_api_node)

    all_witness_names = witness_names_alpha + witness_names_beta

    # Run
    logger.info('Running networks, waiting for live...')
    wait_for_live=True

    alpha_net.connect_with(beta_net)

    logger.info('Running networks, waiting for live...')

    #June 20, 2032 9:45:38 AM
    alpha_net.run(wait_for_live, environment_variables_alpha)

    #1654069301 = June 1, 2022 7:41:41 AM
    beta_net.run(wait_for_live, environment_variables_beta)

    prepare_witnesses(init_node, api_node, all_witness_names)

@pytest.fixture(scope="package")
def world_during_hf25():
    logger.info('Preparing fixture world_with_witnesses')
    with World(directory=context.get_current_directory()) as world:
        #for debug purposes
        network_number = 0

        #November 8, 2023 7:41:40 AM
        prepare_world(world, network_number, environment_variables={"HIVE_HF26_TIME": "1699429300"})
        yield world

@pytest.fixture(scope="package")
def world_during_hf26():
    logger.info('Preparing fixture world_with_witnesses')
    with World(directory=context.get_current_directory()) as world:
        #for debug purposes
        network_number = 1

        #June 1, 2022 7:41:41 AM
        prepare_world(world, network_number, environment_variables={"HIVE_HF26_TIME": "1654069301"})
        yield world

@pytest.fixture(scope="package")
def world_during_hf26_without_majority():
    logger.info('Preparing fixture world_with_witnesses')
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)

        #for debug purposes
        network_number_alpha    = 2
        network_number_beta     = 0

        #1971337538 = June 20, 2032 9:45:38 AM
        #1654069301 = June 1, 2022 7:41:41 AM
        prepare_world_with_2_sub_networks(world, network_number_alpha, network_number_beta, environment_variables_alpha={"HIVE_HF26_TIME": "1971337538"}, environment_variables_beta={"HIVE_HF26_TIME": "1654069301"})
        yield world
