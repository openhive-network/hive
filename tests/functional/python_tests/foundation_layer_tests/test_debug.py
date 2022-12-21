import pytest
import test_tools as tt

from ....local_tools import run_for

class VestPrice:
    def __init__(self, quote : tt.Asset.Vest, base : tt.Asset.Test) -> None:
        self.base = base
        self.quote = quote

    @classmethod
    def from_dgpo(self, dgpo : dict):
        return self(quote=tt.Asset.Vest(dgpo['total_vesting_shares']['amount']),
          base=tt.Asset.Test(dgpo['total_vesting_fund_hive']['amount']))
    
    def __str__(self) -> str:
        q = int(self.quote.amount)
        b = int(self.base.amount)
        p = q/b
        p /= 10**self.base.precision
        return f"{p} {self.quote.token}/{self.base.token}"

    def as_nai(self) -> dict:
        return dict({
            "quote": self.quote.as_nai(),
            "base": self.base.as_nai()
        })


    def __repr__(self) -> str:
        return str(self.as_nai())
 
def report_vest_price(node, msg : str):
    dgpo = node.api.database.get_dynamic_global_properties()
    
    tt.logger.info(f"{msg} vests price {dgpo['total_vesting_shares']}/{dgpo['total_vesting_fund_hive']}.")

    evaluated_price = int(dgpo['total_vesting_shares']['amount'])/int(dgpo['total_vesting_fund_hive']['amount'])
    tt.logger.info(f"{msg} vests price (evaluated): {evaluated_price}.")

    cvp = VestPrice.from_dgpo(dgpo=dgpo)
    tt.logger.info(f"{msg} vests price (wrapped): {cvp}.")

@run_for('testnet')
def test_debug(node):
    node.wait_number_of_blocks(1)
    tt.logger.info('transfer to VESTing')
    wallet = tt.Wallet(attach_to=node)
    
    # this is required to minimanize inflation impact on vest price
    wallet.api.transfer_to_vesting(
        from_='initminer',
        to='initminer',
        amount=tt.Asset.Test(10000)
    ) 
    node.wait_number_of_blocks(1)

    report_vest_price(node, 'Current')

    new_price = VestPrice(quote=tt.Asset.Vest(10), base=tt.Asset.Test(10))
    tt.logger.info(f"new vests price {new_price}.")
    tt.logger.info(f"new vests price {new_price.as_nai()}.")
    #node.api.debug_node.debug_set_vest_price(vest_price=new_price.as_nai())
    
    new_price_d = dict(quote = tt.Asset.Vest(10).as_nai(), base=tt.Asset.Test(10).as_nai())
    tt.logger.info(f"Attempting to set new vests price {new_price_d}.")
    node.api.debug_node.debug_set_vest_price(vest_price=new_price_d)
    
    node.wait_number_of_blocks(1)
 
    report_vest_price(node, 'Changed')
