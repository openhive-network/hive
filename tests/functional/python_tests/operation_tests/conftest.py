import pytest

import test_tools as tt
from hive_local_tools.functional import VestPrice


@pytest.fixture
def speed_up_node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.run(time_offset="+0h x5")
    return node


@pytest.fixture
def wallet(speed_up_node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=speed_up_node)


@pytest.fixture
def prepared_node(speed_up_node: tt.InitNode, wallet: tt.Wallet) -> tt.InitNode:
    wallet.api.update_witness('initminer', 'http://url.html',
                              tt.Account('initminer').public_key,
                              {'account_creation_fee': tt.Asset.Test(3)}
                              )

    new_price = VestPrice(quote=tt.Asset.Vest(1800), base=tt.Asset.Test(1))
    tt.logger.info(f"new vests price {new_price}.")
    tt.logger.info(f"new vests price {new_price.as_nai()}.")
    speed_up_node.api.debug_node.debug_set_vest_price(vest_price=new_price.as_nai())
    wallet.api.transfer_to_vesting("initminer", "initminer", tt.Asset.Test(10_000_000))

    speed_up_node.wait_number_of_blocks(43)

    dgpo = speed_up_node.api.wallet_bridge.get_dynamic_global_properties()
    total_vesting_shares = dgpo['total_vesting_shares']["amount"]
    total_vesting_fund_hive = dgpo['total_vesting_fund_hive']["amount"]
    tt.logger.info(f"total_vesting_shares: {int(total_vesting_shares)}")
    tt.logger.info(f"total_vesting_fund_hive: {int(total_vesting_fund_hive)}")
    tt.logger.info(f"VESTING_PRICE: {int(total_vesting_shares) // int(total_vesting_fund_hive)}")

    return speed_up_node

