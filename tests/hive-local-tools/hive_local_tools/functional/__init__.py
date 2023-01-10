from __future__ import annotations

import test_tools as tt

class VestPrice:
    def __init__(self, quote: tt.Asset.Vest, base: tt.Asset.Test) -> None:
        self.base = base
        self.quote = quote

    def __str__(self) -> str:
        ratio = self.quote.amount / self.base.amount / 10 ** self.base.precision
        return f"{ratio} {self.quote.token} per 1 {self.base.token}"

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}({self.as_nai()})"

    @classmethod
    def from_dgpo(cls, dgpo: dict) -> VestPrice:
        vest: tt.Asset.Vest = tt.Asset.from_(dgpo['total_vesting_shares'])
        hive: tt.Asset.Test = tt.Asset.from_(dgpo['total_vesting_fund_hive'])
        return cls(quote=vest, base=hive)

    def as_nai(self) -> dict:
        return {
            "quote": self.quote.as_nai(),
            "base": self.base.as_nai()
        }
