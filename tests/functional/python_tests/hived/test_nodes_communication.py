from test_tools import Wallet


def test_data_propagation_between_nodes_via_p2p(world):
    network = world.create_network()
    init_node = network.create_init_node()
    api_node = network.create_api_node()
    network.run()

    # Create account on init node
    wallet = Wallet(attach_to=init_node)
    wallet.api.create_account('initminer', 'alice', '{}')

    # Check if account was send via P2P and is present on API node
    response = api_node.api.database.list_accounts(start='', limit=100, order='by_name')
    assert 'alice' in [account['name'] for account in response['accounts']]
