import test_tools as tt

class VestPrice:
    def __init__(self, quote: tt.Asset.Vest, base: tt.Asset.Test) -> None:
        self.base = base
        self.quote = quote

    @classmethod
    def from_dgpo(self, dgpo: dict):
        return self(quote=tt.Asset.Vest(dgpo['total_vesting_shares']['amount']),
                    base=tt.Asset.Test(dgpo['total_vesting_fund_hive']['amount']))

    def __str__(self) -> str:
        q = int(self.quote.amount)
        b = int(self.base.amount)
        p = q / b
        p /= 10 ** self.base.precision
        return f"{p} {self.quote.token}/{self.base.token}"

    def as_nai(self) -> dict:
        return dict({
            "quote": self.quote.as_nai(),
            "base": self.base.as_nai()
        })

    def __repr__(self) -> str:
        return str(self.as_nai())