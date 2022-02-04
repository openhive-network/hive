from pathlib import Path

from test_tools import logger, Wallet, Asset
from threading import Thread

from local_tools import run_networks


START_TEST_BLOCK = 108


def test_x(world):
    alpha_network = world.create_network()
    beta_network = world.create_network()
    alpha_node = alpha_network.create_init_node()
    beta_node = beta_network.create_api_node()
    alpha_network.connect_with(beta_network)
    alpha_network.run()
    beta_network.run()

    alpha_wallet = Wallet(attach_to=alpha_node)


    alpha_wallet.api.create_account('initminer', 'alice', '{}')
    alpha_wallet.api.create_account('initminer', 'bob', '{}')
    alpha_wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.001))
    alpha_wallet.api.transfer('initminer', 'alice', Asset.Test(30), "asd")
    for i in range(28):
        Thread(target = lambda: alpha_wallet.api.transfer('alice', 'bob', Asset.Test(1), f'burn rc number {i}'), daemon=True).start()

    alpha_node.wait_number_of_blocks(1)
        

    transaction1 = alpha_wallet.api.transfer('alice', 'bob', Asset.Test(1), 'dummy operation1', broadcast=False)
    transaction2 = alpha_wallet.api.transfer('alice', 'bob', Asset.Test(1), 'dummy operation2', broadcast=False)
    transaction1 = alpha_wallet.api.sign_transaction(transaction1, broadcast=False)
    transaction2 = alpha_wallet.api.sign_transaction(transaction2, broadcast=False)

    alpha_network.disconnect_from(beta_network)

    alpha_node.api.condenser.broadcast_transaction(transaction1)
    try:
        alpha_node.api.condenser.broadcast_transaction(transaction2)
    except:
        pass

    beta_node.api.condenser.broadcast_transaction(transaction2)
    try:
        beta_node.api.condenser.broadcast_transaction(transaction1)
    except:
        pass

    alpha_network.connect_with(beta_network)

    logger.info('Ready!')
    while True:
        pass


