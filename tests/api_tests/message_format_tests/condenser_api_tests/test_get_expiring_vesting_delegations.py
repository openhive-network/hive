import test_tools as tt

from ..local_tools import create_account_and_fund_it, create_and_cancel_vesting_delegation, date_from_now, run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_expiring_vesting_delegations(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)

        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account('initminer', 'bob', '{}')
        create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    node.api.condenser.get_expiring_vesting_delegations('alice', date_from_now(weeks=0))
