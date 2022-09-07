import test_tools as tt


def test_get_transaction_reversible():
    net = tt.Network()
    init_node = tt.InitNode(network=net)
    api_node = tt.ApiNode(network=net)

    tt.logger.info('Running network, waiting for live sync...')

    # PREREQUISITES
    net.run()
    wallet = tt.Wallet(attach_to=init_node)

    # TRIGGER
    trx = wallet.api.transfer_to_vesting('initminer', 'initminer', tt.Asset.Test(0.001))
    api_node.wait_number_of_blocks(1)
    irreversible = api_node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]

    # VERIFY
    assert irreversible < trx["block_num"]

    response = api_node.api.account_history.get_transaction(id=trx["transaction_id"], include_reversible=True)
    assert response["transaction_id"] == trx["transaction_id"]
