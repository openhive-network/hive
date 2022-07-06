import pytest

import test_tools as tt

from .local_tools import import_keys
from ......local_tools import create_account_and_fund_it

@pytest.mark.test
def test_cancel_transfer_from_savings(nai_wallet):
    import_keys(nai_wallet)
    nai_wallet.api.cancel_transfer_from_savings('alice', 1, broadcast=False)
