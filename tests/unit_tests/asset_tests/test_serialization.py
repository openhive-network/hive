from test_tools import Asset


def test_string_conversions():
    for asset, expected in [
        (Asset.Hbd(1000), '1000.000 HBD'),
        (Asset.Hive(1000), '1000.000 HIVE'),
        (Asset.Vest(1000), '1000.000000 VESTS'),
    ]:
        assert str(asset) == expected


def test_nai_conversions():
    def nai(amount, precision, symbol):
        # Example of nai format for '1.000 HIVE':
        # {
        #   "amount": "1000",
        #   "precision": 3,
        #   "nai": "@@000000021"
        # }
        return {'amount': str(amount), 'precision': precision, 'nai': symbol}

    assert Asset.Hbd(1000).as_nai() == nai(1000_000, 3, '@@000000013')
    assert Asset.Hive(1000).as_nai() == nai(1000_000, 3, '@@000000021')
    assert Asset.Vest(1000).as_nai() == nai(1000_000000, 6, '@@000000037')
