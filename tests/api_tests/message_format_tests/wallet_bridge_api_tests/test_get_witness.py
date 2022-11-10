import pytest

import test_tools as tt

from ..local_tools import as_string

from .block_log.generate_block_log import WITNESSES_NAMES


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
def test_get_witness_with_correct_value(replayed_node, witness_account):
    replayed_node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.parametrize(
    'witness_account', [

        ['example-array']
    ]
)
def test_get_witness_with_incorrect_type_of_argument(node, witness_account):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_witness(witness_account)
