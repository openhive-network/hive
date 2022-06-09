import pytest

from typing import Dict, Optional

from test_tools import Account, Asset, constants, logger, Wallet, World
from test_tools.private.scope import context

def prepare_world(world : World, environment_variables: Optional[Dict] = None):
    world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)

    all_witness_names = [f'witness{i}-alpha' for i in range(20)]

    # Create first network
    alpha_net = world.create_network('Alpha')
    init_node = alpha_net.create_init_node()
    alpha_net.create_witness_node(witnesses=[f'witness{i}-alpha' for i in range(0, 6)])
    alpha_net.create_witness_node(witnesses=[f'witness{i}-alpha' for i in range(6, 11)])
    alpha_net.create_witness_node(witnesses=[f'witness{i}-alpha' for i in range(11, 15)])
    alpha_net.create_witness_node(witnesses=[f'witness{i}-alpha' for i in range(15, 20)])
    api_node = alpha_net.create_api_node()

    # Run
    logger.info('Running networks, waiting for live...')
    wait_for_live=True
    alpha_net.run(wait_for_live, environment_variables)

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

@pytest.fixture(scope="package")
def world_before_hf26():
    """
    Fixture consists of world with 1 init node, 4 witness nodes and 1 api node.
    After fixture creation there are 21 active witnesses, and last irreversible block
    is behind head block like in real network.
    """

    logger.info('Preparing fixture world_with_witnesses')
    with World(directory=context.get_current_directory()) as world:
        #November 8, 2023 7:42:03 AM
        prepare_world(world, environment_variables={"HIVE_HF26_TIME": "1699429323"})
        yield world

@pytest.fixture(scope="package")
def world_after_hf26():
    """
    Fixture consists of world with 1 init node, 4 witness nodes and 1 api node.
    After fixture creation there are 21 active witnesses, and last irreversible block
    is behind head block like in real network.
    """

    logger.info('Preparing fixture world_with_witnesses')
    with World(directory=context.get_current_directory()) as world:
        #June 1, 2022 7:42:03 AM
        prepare_world(world, environment_variables={"HIVE_HF26_TIME": "1654069323"})
        yield world
