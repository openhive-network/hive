def test_get_witness_schedule_with_correct_value_false(replayed_node):
    replayed_node.api.wallet_bridge.get_witness_schedule(False)


def test_get_witness_schedule_with_correct_value_true(replayed_node):
    replayed_node.api.wallet_bridge.get_witness_schedule(True)
