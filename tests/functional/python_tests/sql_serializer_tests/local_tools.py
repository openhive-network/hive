from sqlalchemy_utils import database_exists, create_database, drop_database

from test_tools import logger, Wallet


BLOCKS_IN_FORK = 5
BLOCKS_AFTER_FORK = 10


def make_fork(world, fork_number=0):
    alpha_net = world.network('Alpha')
    beta_net = world.network('Beta')
    alpha_witness_node = alpha_net.node('WitnessNode0')
    beta_witness_node = beta_net.node(name='WitnessNode0')

    main_chain_wallet = Wallet(attach_to=alpha_witness_node)
    fork_chain_wallet = Wallet(attach_to=beta_witness_node)
    with main_chain_wallet.in_single_transaction():
        name = "before-acnt-" + str(fork_number)
        main_chain_wallet.api.create_account('initminer', name, '')
    fork_block = beta_witness_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    head_block = fork_block
    alpha_net.disconnect_from(beta_net)

    with fork_chain_wallet.in_single_transaction():
        name = "dummy-acnt-" + str(fork_number)
        fork_chain_wallet.api.create_account('initminer', name, '')

    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK)
    alpha_net.connect_with(beta_net)
    for node in [alpha_witness_node, beta_witness_node]:
        node.wait_for_block_with_number(head_block + BLOCKS_IN_FORK + BLOCKS_AFTER_FORK)

    head_block = beta_witness_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    return head_block


def wait_for_irreversible_progress(node, block_num):
    logger.info(f'Waiting for progress of irreversible block')
    head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
    irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    logger.info(f"Current head_block_number: {head_block_number}, irreversible_block_num: {irreversible_block_num}")
    while irreversible_block_num < block_num:
        node.wait_for_block_with_number(head_block_number+1)
        head_block_number = node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
        irreversible_block_num = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    return irreversible_block_num, head_block_number
