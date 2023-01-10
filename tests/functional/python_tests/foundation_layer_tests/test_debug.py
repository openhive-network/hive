from typing import Final

import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.functional import VestPrice


@run_for('testnet')
def test_vesting(node):
    vest_per_hive_ratio: Final[int] = 100
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(name="alice", hives=tt.Asset.Test(1))

    # this is required to minimize inflation impact on vest price
    wallet.api.transfer_to_vesting(
        from_="initminer",
        to="initminer",
        amount=tt.Asset.Test(10000)
    )
    node.wait_number_of_blocks(1)

    new_price = VestPrice(quote=tt.Asset.Vest(vest_per_hive_ratio), base=tt.Asset.Test(1))
    report_vest_price(node, "before price updating")
    tt.logger.info(f"new vests price {new_price}.")
    node.api.debug_node.debug_set_vest_price(vest_price=new_price.as_nai())
    report_vest_price(node, "after price updating")

    wallet.api.transfer_to_vesting("alice", "alice", tt.Asset.Test(1))

    assert node.api.condenser.get_accounts(["alice"])[0]["vesting_shares"] == tt.Asset.Vest(vest_per_hive_ratio)



def report_vest_price(node, msg: str):
    dgpo = node.api.database.get_dynamic_global_properties()

    tt.logger.info(f"{msg} vests price (from dgpo): {dgpo['total_vesting_shares']}/{dgpo['total_vesting_fund_hive']}.")
    tt.logger.info(f"{msg} vests price (wrapped): {VestPrice.from_dgpo(dgpo=dgpo)}.")
