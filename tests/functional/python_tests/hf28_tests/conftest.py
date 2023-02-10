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


@pytest.fixture
def prepare_environment_on_hf_27(node):
    node = tt.InitNode()
    # run on a node with a date earlier than the start date of hardfork 28 (february 8, 2023 1:00:00 am)
    node.run(time_offset='@2023-01-07 10:10:10')
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)

    assert node.api.condenser.get_hardfork_version() == "1.27.0"
    return node, wallet
