import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_comments(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('herverisson', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.post_comment('herverisson', 'homefront-a-one-act-play-last-part', '', 'someone', 'test-title', 'this is a body', '{}')
    comments = node.api.database.find_comments(comments=[['herverisson', 'homefront-a-one-act-play-last-part']])['comments']
    assert len(comments) != 0
