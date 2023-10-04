"""Scenarios description: https://gitlab.syncad.com/hive/hive/-/issues/523"""

from hive_local_tools.functional.python.operation.recurrent_transfer import RecurrentTransferDefinition
from hive_local_tools.constants import MIN_RECURRENT_TRANSFERS_RECURRENCE


RECURRENT_TRANSFER_DEFINITIONS = [
    RecurrentTransferDefinition(10, 2 * MIN_RECURRENT_TRANSFERS_RECURRENCE, 2, 1),
    RecurrentTransferDefinition(20, MIN_RECURRENT_TRANSFERS_RECURRENCE, 3, 2),
    RecurrentTransferDefinition(30, 2 * MIN_RECURRENT_TRANSFERS_RECURRENCE, 2, 3),
]
