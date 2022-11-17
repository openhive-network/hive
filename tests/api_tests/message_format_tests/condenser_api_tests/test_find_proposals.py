import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_proposals(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'alice', '{}')

        wallet.api.post_comment('initminer', 'test-permlink', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
        wallet.api.create_proposal('initminer', 'initminer', date_from_now(weeks=2), date_from_now(weeks=50),
                                   tt.Asset.Tbd(5), 'test subject', 'test-permlink')
    prepared_node.api.condenser.find_proposals([0])
