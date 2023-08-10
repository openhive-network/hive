import test_tools as tt
from hive_local_tools.functional import VestPrice


def test_set_vest_price_debug_node_api_bug_replication():
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)

    wallet.api.update_witness('initminer', 'http://url.html',
                              tt.Account('initminer').public_key,
                              {'account_creation_fee': tt.Asset.Test(3)}
                              )

    new_price = VestPrice(quote=tt.Asset.Vest(1800), base=tt.Asset.Test(1))
    tt.logger.info(f"new vests price {new_price}.")
    tt.logger.info(f"new vests price {new_price.as_nai()}.")
    node.api.debug_node.debug_set_vest_price(vest_price=new_price.as_nai())

    wallet.api.transfer_to_vesting("initminer", "initminer", tt.Asset.Test(10_000_000))
    node.wait_number_of_blocks(1)
