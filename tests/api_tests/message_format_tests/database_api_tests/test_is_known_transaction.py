def test_is_known_transaction(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.database.is_known_transaction(id=transaction['transaction_id'])
