from test_tools import logger, Wallet


def test_p2p_traffic(world):
    net = world.create_network()
    init_node = net.create_init_node()
    api_node = net.create_api_node()

    logger.info('Running network, waiting for live sync...')
    net.run()

    # PREREQUISITES
    wallet = Wallet(attach_to=api_node)
    init_node.wait_for_block_with_number(5)
    api_node.wait_for_block_with_number(5)


    # TRIGGER    
    accounts = get_accounts_name(wallet.create_accounts(100_000, 'receiver'))

    # VERIFY
    # No crash.



def get_accounts_name(accounts):
    accounts_names = []
    for account_number in range(len(accounts)):
        accounts_names.append(accounts[account_number].name)
    return accounts_names
