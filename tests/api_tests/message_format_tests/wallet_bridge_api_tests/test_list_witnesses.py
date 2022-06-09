import pytest

import test_tools as tt

from .block_log.generate_block_log import WITNESSES_NAMES
from .local_tools import as_string


CORRECT_VALUES = [
        # WITNESS ACCOUNT
        (WITNESSES_NAMES[0], 100),
        (WITNESSES_NAMES[-1], 100),
        ('non-exist-acc', 100),
        ('true', 100),
        ('', 100),
        (100, 100),
        (True, 100),
        ('100', 100),

        # LIMIT
        (WITNESSES_NAMES[0], 0),
        (WITNESSES_NAMES[0], 1000),
]


@pytest.mark.parametrize(
    'witness_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (WITNESSES_NAMES[0], True),  # bool is treated like numeric (0:1)
    ]
)
def test_list_witnesses_with_correct_value(witness_account, limit, replayed_node):
    replayed_node.api.wallet_bridge.list_witnesses(witness_account, limit)


@pytest.mark.parametrize(
    'witness_account, limit', [
        # LIMIT
        (WITNESSES_NAMES[0], -1),
        (WITNESSES_NAMES[0], 1001),
    ]
)
def test_list_witnesses_with_incorrect_value(replayed_node, witness_account, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        replayed_node.api.wallet_bridge.list_witnesses(witness_account, limit)


@pytest.mark.parametrize(
    'witness_account, limit', [
        # WITNESS ACCOUNT
        ([WITNESSES_NAMES[0]], 100),

        # LIMIT
        (WITNESSES_NAMES[0], 'incorrect_string_argument'),
        (WITNESSES_NAMES[0], 'true'),
    ]
)
def test_list_witnesses_with_incorrect_type_of_arguments(replayed_node, witness_account, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        replayed_node.api.wallet_bridge.list_witnesses(witness_account, limit)

