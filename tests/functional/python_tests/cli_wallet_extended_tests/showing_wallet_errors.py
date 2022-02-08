def test_showing_broadcast_transaction_synchronous_errors(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction = wallet.api.sign_transaction(transaction, broadcast=False)

    transaction['operations'][0] = {
        'type': 'account_create_operation',
        'value': transaction['operations'][0][1]
    }

    transaction['operations'][0]['value']['fee'] = {
        "amount": "0",
        "precision": 3,
        "nai": "@@000000021"
    }

    #Data in transaction(variable) must be changed for the broadcast_transaction_synchronous() method to work correctly.
    node.api.wallet_bridge.broadcast_transaction_synchronous(transaction)


def test_showing_sign_transaction_errors(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)

    # The first part of the changes required for the correct operation of the method sign_transaction()
    transaction['operations'][0] = {
        'type': 'account_create_operation',
        'value': transaction['operations'][0][1]
    }

    # Second part of the changes required for the correct operation of the method sign_transaction()
    transaction['operations'][0]['value']['fee'] = {
        "amount": "0",
        "precision": 3,
        "nai": "@@000000021"
    }
    # Changing data to data accepted by the broadcast_transaction_synchronous() method does not work with the sign_transaction() method
    transaction = wallet.api.sign_transaction(transaction, broadcast=False)

