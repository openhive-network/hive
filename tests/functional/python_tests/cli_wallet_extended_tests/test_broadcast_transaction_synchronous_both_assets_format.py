from test_tools import Asset


def test_broadcast_transaction_synchronous_support_legacy_asset_format(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)

    asset_in_legacy_format = str(Asset.Test(0))
    transaction['operations'][0][1]['fee'] = asset_in_legacy_format

    node.api.wallet_bridge.broadcast_transaction_synchronous(transaction)


def test_broadcast_transaction_synchronous_support_nai_asset_format(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)

    transaction['operations'][0] = {
        'type': 'account_create_operation',
        'value': transaction['operations'][0][1]
    }
    asset_in_nai_format = Asset.Test(0).as_nai()
    transaction['operations'][0]['value']['fee'] = asset_in_nai_format

    node.api.wallet_bridge.broadcast_transaction_synchronous(transaction)
