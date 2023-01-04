import pytest

import test_tools as tt
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT, PROXY_ACCOUNT

@pytest.fixture
def prepare_environment(node):
    node.restart(time_offset="+0 x5")
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)

    return node, wallet
