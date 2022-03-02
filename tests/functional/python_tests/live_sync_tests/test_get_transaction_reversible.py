from test_tools import logger, Wallet, Asset


def test_get_transaction_reversible(world):
    net = world.create_network()
    init_node = net.create_init_node()
    api_node = net.create_api_node()

    logger.info('Running network, waiting for live sync...')

    # PREREQUISITES
    net.run()
    wallet = Wallet(attach_to=init_node)

    # TRIGGER
    trx = wallet.api.transfer_to_vesting('initminer', 'initminer', Asset.Test(0.001))
    api_node.wait_number_of_blocks(1)
    irreversible = api_node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]

    # VERIFY
    assert irreversible < trx["block_num"]

    response = api_node.api.account_history.get_transaction(id=trx["transaction_id"], include_reversible=True)
    assert response["transaction_id"] == trx["transaction_id"]
