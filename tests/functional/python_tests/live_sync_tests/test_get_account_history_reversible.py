from test_tools import logger, Wallet, Asset
import json


def test_get_account_history_reversible(world):
    net = world.create_network()
    init_node = net.create_init_node()
    api_node = net.create_api_node()

    logger.info('Running network, waiting for live sync...')

    # PREREQUISITES
    net.run()
    wallet = Wallet(attach_to=init_node)
    logger.info('wallet started...')

    # TRIGGER
    wallet.api.create_account('initminer', 'alice', '{}')
    trx = wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.001))

    irreversible = api_node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]
    logger.info(f'irreversible {irreversible}')

    # VERIFY
    assert irreversible < trx["block_num"]

    response = api_node.api.account_history.get_account_history(account='alice', start=-1, limit=1000, include_reversible=True)
    op_types = [entry[1]["op"]["type"] for entry in response["history"]]

    assert 'transfer_to_vesting_operation' in op_types
    assert 'transfer_to_vesting_completed_operation' in op_types

    while irreversible < trx["block_num"]:
        api_node.wait_number_of_blocks(1)
        irreversible = api_node.api.database.get_dynamic_global_properties()["last_irreversible_block_num"]

    response = api_node.api.account_history.get_account_history(account='alice', start=-1, limit=1000, include_reversible=True)
    op_types = [entry[1]["op"]["type"] for entry in response["history"]]

    assert 'transfer_to_vesting_operation' in op_types
    assert 'transfer_to_vesting_completed_operation' in op_types
