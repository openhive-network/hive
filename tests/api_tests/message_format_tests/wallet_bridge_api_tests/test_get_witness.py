import pytest

from test_tools import exceptions

from .local_tools import as_string, prepare_node_with_witnesses


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer

CORRECT_VALUES = [
    WITNESSES_NAMES[0],
    WITNESSES_NAMES[-1],
    'non-exist-acc',
    '',
    100,
    True,
]


@pytest.mark.parametrize(
    'witness_account', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@pytest.mark.testnet
def test_get_witness_with_correct_value(world, witness_account):
    node = prepare_node_with_witnesses(world, WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.remote_node_5m
def test_get_witness_with_correct_value_5m(node5m):
    node5m.api.wallet_bridge.get_witness('gtg')


@pytest.mark.remote_node_64m
def test_get_witness_with_correct_value_64m(node64m):
    node64m.api.wallet_bridge.get_witness('gtg')


@pytest.mark.testnet
def test_get_witness_with_correct_value(world, witness_account):
    node = prepare_node_with_witnesses(world, WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.parametrize(
    'witness_account', [

        ['example-array']
    ]
)
@pytest.mark.testnet
def test_get_witness_with_incorrect_type_of_argument(node, witness_account):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_witness(witness_account)
