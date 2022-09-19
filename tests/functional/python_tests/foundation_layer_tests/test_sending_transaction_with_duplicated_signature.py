def test_sending_transaction_with_duplicated_signature(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction['signatures'] = 2 * transaction['signatures']  # Duplicate signature
    node.api.wallet_bridge.broadcast_transaction(transaction)
