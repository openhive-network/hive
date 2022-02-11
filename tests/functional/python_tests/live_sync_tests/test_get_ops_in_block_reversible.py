from test_tools import logger, Wallet, Asset


def test_get_ops_in_block_reversible(world):
    net = world.create_network()
    net.create_init_node()
    api_node = net.create_api_node()

    logger.info('Running network, waiting for live sync...')

    # PREREQUISITES
    net.run()
    wallet = Wallet(attach_to=api_node)

    # TRIGGER
    trx = wallet.api.transfer_to_vesting('initminer', 'initminer', Asset.Test(0.001))
    api_node.wait_number_of_blocks(1)
    irreversible = api_node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]

    # VERIFY
    assert irreversible < trx["block_num"]

    response = api_node.api.account_history.get_ops_in_block(block_num=trx["block_num"], only_virtual=False, include_reversible=True)
    op_types = [op["op"]["type"] for op in response["ops"]]

    assert 'transfer_to_vesting_operation' in op_types
