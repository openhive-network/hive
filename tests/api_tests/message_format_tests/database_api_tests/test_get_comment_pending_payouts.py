import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_comment_pending_payouts(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('herverisson', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.post_comment('herverisson', 'homefront-a-one-act-play-last-part', '', 'test-parent-permlink',
                                'test-title', 'test-body', '{}')
    cashout_info = node.api.database.get_comment_pending_payouts(comments=[
        ['herverisson', 'homefront-a-one-act-play-last-part']])['cashout_infos']
    assert len(cashout_info) != 0
