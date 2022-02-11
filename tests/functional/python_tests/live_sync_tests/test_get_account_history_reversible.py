from test_tools import logger, Wallet, Asset


def test_get_account_history_reversible(world):
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

    response = api_node.api.account_history.get_account_history(account='initminer', start=-1, limit=1000, include_reversible=True)
    ops = [entry[1] for entry in response["history"]]
    op_types = [op["op"]["type"] for op in ops]

    assert 'transfer_to_vesting_operation' in op_types
