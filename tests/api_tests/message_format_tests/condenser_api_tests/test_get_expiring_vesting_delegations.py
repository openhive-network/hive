import test_tools as tt

from ..local_tools import create_and_cancel_vesting_delegation, run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_expiring_vesting_delegations(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account('initminer', 'bob', '{}')
        create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    prepared_node.api.condenser.get_expiring_vesting_delegations('alice', date_from_now(weeks=0))
