import test_tools as tt


def test_data_propagation_between_nodes_via_p2p():
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)
    network.run()

    # Create account on init node
    wallet = tt.Wallet(attach_to=init_node)
    wallet.api.create_account("initminer", "alice", "{}")

    # Check if account was send via P2P and is present on API node
    response = api_node.api.database.list_accounts(start="", limit=100, order="by_name")
    assert "alice" in [account["name"] for account in response["accounts"]]
