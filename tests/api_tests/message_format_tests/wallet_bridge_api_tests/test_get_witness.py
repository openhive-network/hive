import pytest

import test_tools as tt

from .local_tools import as_string, prepare_node_with_witnesses, run_for


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
@run_for('testnet')
def test_get_witness_with_correct_value_in_testnet(prepared_node, witness_account):
    node = prepare_node_with_witnesses(WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.parametrize(
    'witness_account', [

        ['example-array']
    ]
)
@run_for('testnet')
def test_get_witness_with_incorrect_type_of_argument(prepared_node, witness_account):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_witness(witness_account)
