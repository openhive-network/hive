from test_tools import logger, Wallet


def test_account_creation(world):
    network = world.create_network()
    init_node = network.create_init_node()
    api_node = network.create_api_node()
    network.run()

    wallet = Wallet(attach_to=init_node)

    logger.info(api_node.get_http_endpoint())

    i = 0
    while True:
        with wallet.in_single_transaction():
            NUMBER_OF_ACCOUNTS_CREATED_IN_ONE_BLOCK = 3
            for j in range(NUMBER_OF_ACCOUNTS_CREATED_IN_ONE_BLOCK):
                wallet.api.create_account('initminer', f'alice{j + NUMBER_OF_ACCOUNTS_CREATED_IN_ONE_BLOCK * i}', '{}')
        i += 1
