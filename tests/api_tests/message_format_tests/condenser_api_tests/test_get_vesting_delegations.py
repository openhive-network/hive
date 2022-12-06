import test_tools as tt

from hive_local_tools import run_for


# This test is not performed on 5 million block log because it doesn't contain any vesting delegations.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'live_mainnet')
def test_get_vesting_delegations(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    delegations = node.api.condenser.get_vesting_delegations('coar', '', 100)
    assert len(delegations) != 0


@run_for('testnet', 'live_mainnet')
def test_get_vesting_delegations_with_default_third_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    delegations = node.api.condenser.get_vesting_delegations('coar', '')
    assert len(delegations) != 0


def preparation_for_testnet_node(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('coar', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

        wallet.api.create_account('coar', 'tipu', '{}')
        wallet.api.delegate_vesting_shares('coar', 'tipu', tt.Asset.Vest(5))
