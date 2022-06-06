from test_tools import Asset


def test_asset_serialization(node, wallet):
    wallet.api.transfer('initminer', 'hive.fund', Asset.Hive(100), 'memo')
    pass
