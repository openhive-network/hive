from __future__ import annotations

import pytest
from dataclasses import dataclass, field
from typing import Literal
import test_tools as tt
from hive_local_tools.functional import VestPrice


@dataclass
class LimitOrderAccount:
    acc_info: dict = field(init=False, default_factory=dict)
    name: str
    node: tt.InitNode
    wallet: tt.Wallet

    def assert_balance(self, *, amount: int | float, check_hbd: bool, message: Literal["expiration", "creation",
                                                                                       "order_match", "no_match"]):
        self.__update_account_info()
        get_balance = self.__get_hbd_balance if check_hbd else self.__get_hive_balance
        currency = tt.Asset.Tbd if check_hbd else tt.Asset.Test

        messages = {
            "expiration": (
                f"Something went wrong after expiration of orders. {self.name} should have {amount} {currency.token}."
            ),
            "creation": f"{currency} balance of {self.name} wasn't reduced after creating order.",
            "order_match": (
                f"Something went wrong in completing a trade. {currency.token} balance of {self.name} should be equal"
                f" to {amount}."
            ),
            "no_match": (
                f"Something went wrong after order creation - it shouldn't be matched. {currency.token} balance of"
                f" {self.name} should be equal to {amount}."
            ),
        }

        assert get_balance() == currency(amount), messages[message]

    def assert_not_completed_order(self, amount: int, hbd: bool):
        query = self.node.api.database.find_limit_orders(account=self.name)["orders"][0]
        for_sale = query["for_sale"]
        nai = query["sell_price"]["base"]
        nai["amount"] = for_sale
        currency = tt.Asset.Tbd if hbd else tt.Asset.Test
        assert tt.Asset.from_(nai) == currency(amount), (
            f"Amount of {currency.token} that are still available for sale "
            f"is not correct. {self.name} should have now {amount} {currency.token}"
        )

    def create_order(
            self,
            amount_to_sell: int,
            min_to_receive: int,
            *,
            order_id: int = 0,
            fill_or_kill: bool = False,
            expiration: int = 60,
            buy_hbd: bool = None
    ):
        return self.wallet.api.create_order(
            self.name,
            order_id,
            tt.Asset.Test(amount_to_sell) if buy_hbd else tt.Asset.Tbd(amount_to_sell),
            tt.Asset.Tbd(min_to_receive) if buy_hbd else tt.Asset.Test(min_to_receive),
            fill_or_kill,
            expiration
        )

    def __get_hbd_balance(self):
        return tt.Asset.from_(self.acc_info['hbd_balance'])

    def __get_hive_balance(self):
        return tt.Asset.from_(self.acc_info['balance'])

    def __update_account_info(self):
        self.acc_info = self.node.api.database.find_accounts(accounts=[self.name])["accounts"][0]


@pytest.fixture
def alice(prepared_node, wallet):
    wallet.create_account("alice", hives=450, hbds=450, vests=50)
    return LimitOrderAccount("alice", prepared_node, wallet)


@pytest.fixture
def bob(prepared_node, wallet):
    wallet.create_account("bob", hives=300, hbds=300, vests=50)
    return LimitOrderAccount("bob", prepared_node, wallet)


@pytest.fixture
def carol(prepared_node, wallet):
    wallet.create_account("carol", hives=400, hbds=400, vests=50)
    return LimitOrderAccount("carol", prepared_node, wallet)


@pytest.fixture
def daisy(prepared_node, wallet):
    wallet.create_account("daisy", hives=480, hbds=480, vests=50)
    return LimitOrderAccount("daisy", prepared_node, wallet)


@pytest.fixture
def elizabeth(prepared_node, wallet):
    wallet.create_account("elizabeth", hives=600, hbds=600, vests=50)
    return LimitOrderAccount("elizabeth", prepared_node, wallet)


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

