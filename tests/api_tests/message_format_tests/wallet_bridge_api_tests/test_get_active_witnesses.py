from .local_tools import prepare_node_with_witnesses


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer


def test_get_active_witnesses_with_correct_value():
    node = prepare_node_with_witnesses(WITNESSES_NAMES)
    node.api.wallet_bridge.get_active_witnesses()
