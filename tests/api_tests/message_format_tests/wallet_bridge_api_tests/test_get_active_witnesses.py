def test_get_active_witnesses_with_correct_value_false(replayed_node):
    replayed_node.api.wallet_bridge.get_active_witnesses(False)


def test_get_active_witnesses_with_correct_value_true(replayed_node):
    replayed_node.api.wallet_bridge.get_active_witnesses(True)
