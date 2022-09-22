def test_get_transaction_hex(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    output_hex = node.api.database.get_transaction_hex(trx=transaction)['hex']
    assert len(output_hex) != 0
