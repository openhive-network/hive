import pytest

import test_tools as tt

from .local_tools import as_string, run_for


WITNESSES_NAMES = ['blocktrades', 'gtg']

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
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
@pytest.mark.prepare_witnesses_in_node_config(WITNESSES_NAMES)
def test_get_witness_with_correct_value(prepared_node, witness_account):
    witness = prepared_node.api.wallet_bridge.get_witness(witness_account)
    if witness is not None:
        assert len(witness) == 21


@pytest.mark.parametrize(
    'witness_account', [

        ['example-array']
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
@pytest.mark.prepare_witnesses_in_node_config(WITNESSES_NAMES)
def test_get_witness_with_incorrect_type_of_argument(prepared_node, witness_account):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_witness(witness_account)
