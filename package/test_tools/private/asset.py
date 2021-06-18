class AssetBase:
    def __init__(self, amount, *, token=None, precision: int, nai: str):
        self.amount = amount * (10 ** precision)
        self.token = token
        self.precision = precision
        self.nai = nai

    def as_nai(self):
        return {
            'amount': str(self.amount),
            'precision': self.precision,
            'nai': self.nai,
        }

    def __eq__(self, other):
        if isinstance(other, str):
            return str(self) == other

        raise TypeError(f'Assets can\'t be compared with objects of type {type(other)}')

    def __str__(self):
        if self.token is None:
            raise RuntimeError(f'Asset with nai={self.nai} hasn\'t string representaion')

        return f'{self.amount / (10 ** self.precision):.{self.precision}f} {self.token}'

    def __repr__(self):
        return f'Asset({self.as_nai()})'

class Asset:
    class Hbd(AssetBase):
        def __init__(self, amount):
            super().__init__(amount, token='HBD', precision=3, nai='@@000000013')

    class Tbd(AssetBase):
        def __init__(self, amount):
            super().__init__(amount, token='TBD', precision=3, nai='@@000000013')

    class Hive(AssetBase):
        def __init__(self, amount):
            super().__init__(amount, token='HIVE', precision=3, nai='@@000000021')

    class Test(AssetBase):
        def __init__(self, amount):
            super().__init__(amount, token='TESTS', precision=3, nai='@@000000021')

    class Vest(AssetBase):
        def __init__(self, amount):
            super().__init__(amount, token='VESTS', precision=6, nai='@@000000037')
