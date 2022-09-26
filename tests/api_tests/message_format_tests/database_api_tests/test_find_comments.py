import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_comments(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('herverisson', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.post_comment('herverisson', 'homefront-a-one-act-play-last-part', '', 'someone', 'test-title', 'this is a body', '{}')
    comments = prepared_node.api.database.find_comments(comments=[['herverisson', 'homefront-a-one-act-play-last-part']])['comments']
    assert len(comments) != 0
