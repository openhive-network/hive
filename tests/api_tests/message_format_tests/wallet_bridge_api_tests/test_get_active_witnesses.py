def test_get_active_witnesses_with_correct_value(replayed_node):
    replayed_node.api.wallet_bridge.get_active_witnesses()
