def test_get_potential_signatures(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = node.api.database.get_potential_signatures(trx=transaction)['keys']
    assert len(keys) != 0
