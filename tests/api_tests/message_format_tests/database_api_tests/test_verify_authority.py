def test_verify_authority(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.database.verify_authority(trx=transaction, pack='hf26')
