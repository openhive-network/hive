from copy import deepcopy
import warnings
from typing import Union


class AssetBase:
    def __init__(self, amount, *, token=None, precision: int, nai: str):
        self.amount = self.__convert_amount_to_internal_representation(amount, precision)
        self.token = token
        self.precision = precision
        self.nai = nai

    @staticmethod
    def __convert_amount_to_internal_representation(amount: Union[int, float], precision: int) -> int:
        AssetBase.__warn_if_precision_might_be_lost(amount, precision)
        return int(round(amount, precision) * pow(10, precision))

    @staticmethod
    def __warn_if_precision_might_be_lost(amount: Union[int, float], precision: int):
        rounded_value = round(amount, precision)
        acceptable_error = 0.1 ** 10

        if abs(amount - rounded_value) > acceptable_error:
            warnings.warn(
                f'Precision lost during asset creation.\n'
                f'\n'
                f'Asset with amount {amount} was requested, but this value was rounded to {rounded_value},\n'
                f'because precision of this asset is {precision} ({pow(0.1, precision):.3f}).'
            )

    def as_nai(self):
        return {
            'amount': str(self.amount),
            'precision': self.precision,
            'nai': self.nai,
        }

    def __add__(self, other):
        self.__assert_same_operands_type(other, 'Can\'t add assets with different tokens or nai')
        result = deepcopy(self)
        result.amount += other.amount
        return result

    def __neg__(self):
        result = deepcopy(self)
        result.amount = -self.amount
        return result

    def __sub__(self, other):
        self.__assert_same_operands_type(other, 'Can\'t subtract assets with different tokens or nai')
        return self + -other

    def __iadd__(self, other):
        self.__assert_same_operands_type(other, 'Can\'t add assets with different tokens or nai')
        self.amount += other.amount
        return self

    def __isub__(self, other):
        self.__assert_same_operands_type(other, 'Can\'t subtract assets with different tokens or nai')
        self.amount -= other.amount
        return self

    def __assert_same_operands_type(self, other, error):
        if type(self) != type(other):
            raise RuntimeError(error)

    def __eq__(self, other):
        if isinstance(other, str):
            return str(self) == other
        elif type(self) == type(other):
            return self.amount == other.amount

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
