import pytest

import test_tools as tt

from .local_tools import import_private_keys_from_json_file
from ......local_tools import create_account_and_fund_it

@pytest.mark.test
def test_cancel_transfer_from_savings(wallet):
    type_of_serialization = wallet[1]
    wallet = wallet[0]
    import_private_keys_from_json_file(wallet, type_of_serialization)
    wallet.api.cancel_transfer_from_savings('alice', 1, broadcast=False)
