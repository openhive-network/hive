import test_tools as tt


def test_get_required_signatures(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = node.api.database.get_required_signatures(trx=transaction, available_keys=[tt.Account('initminer').public_key])['keys']
    assert len(keys) != 0
