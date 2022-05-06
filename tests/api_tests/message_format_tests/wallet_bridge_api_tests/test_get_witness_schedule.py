from .local_tools import prepare_node_with_witnesses


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer


def test_get_witness_schedule_with_correct_value(world):
    node = prepare_node_with_witnesses(world, WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness_schedule()
