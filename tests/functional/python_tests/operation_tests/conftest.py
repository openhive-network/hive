from __future__ import annotations

import pytest
from dataclasses import dataclass
from typing import Literal
import test_tools as tt
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation, Account


@dataclass
class LimitOrderAccount(Account):

    def assert_balance(self, *, amount: int | float, check_hbd: bool, message: Literal["expiration", "creation",
                                                                                       "order_match", "no_match"]):
        super().update_account_info()
        get_balance = self.get_hbd_balance if check_hbd else self.get_hive_balance
        currency = tt.Asset.Tbd if check_hbd else tt.Asset.Test
        messages = {
            "expiration": (
                f"Something went wrong after expiration of orders. {self._name} should have {amount} {currency.token}."
            ),
            "creation": f"{currency} balance of {self._name} wasn't reduced after creating order.",
            "order_match": (
                f"Something went wrong in completing a trade. {currency.token} balance of {self._name} should be equal"
                f" to {amount}."
            ),
            "no_match": (
                f"Something went wrong after order creation - it shouldn't be matched. {currency.token} balance of"
                f" {self._name} should be equal to {amount}."
            ),
        }

        assert get_balance() == currency(amount), messages[message]

    def assert_not_completed_order(self, amount: int, hbd: bool):
        query = self._node.api.database.find_limit_orders(account=self._name)["orders"][0]
        for_sale = query["for_sale"]
        nai = query["sell_price"]["base"]
        nai["amount"] = for_sale
        currency = tt.Asset.Tbd if hbd else tt.Asset.Test
        assert tt.Asset.from_(nai) == currency(amount), (
            f"Amount of {currency.token} that are still available for sale "
            f"is not correct. {self._name} should have now {amount} {currency.token}"
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
        return self._wallet.api.create_order(
            self._name,
            order_id,
            tt.Asset.Test(amount_to_sell) if buy_hbd else tt.Asset.Tbd(amount_to_sell),
            tt.Asset.Tbd(min_to_receive) if buy_hbd else tt.Asset.Test(min_to_receive),
            fill_or_kill,
            expiration
        )

    def create_order_2(
            self,
            amount_to_sell: int,
            min_to_receive: int,
            *,
            order_id: int = 0,
            fill_or_kill: bool = False,
            expiration: int = 60,
            buy_hbd: bool = None):

        expiration_time = tt.Time.serialize(self._node.get_head_block_time() + tt.Time.seconds(expiration),
                                            format_=tt.Time.DEFAULT_FORMAT)
        create_transaction_with_any_operation(
            self._wallet,
            "limit_order_create2",
            owner=self._name,
            orderid=order_id,
            amount_to_sell=tt.Asset.Test(amount_to_sell) if buy_hbd else tt.Asset.Tbd(amount_to_sell),
            exchange_rate={
                "base": tt.Asset.Test(amount_to_sell) if buy_hbd else tt.Asset.Tbd(amount_to_sell),
                "quote": tt.Asset.Tbd(min_to_receive) if buy_hbd else tt.Asset.Test(min_to_receive)
            },
            fill_or_kill=fill_or_kill,
            expiration=expiration_time
        )


@dataclass
class TransferAccount(Account):
    # class with account representation created for transfer, transfer_to/from_savings tests
    def transfer(self, receiver, amount, memo):
        self._wallet.api.transfer(self._name, receiver, amount, memo)

    def transfer_to_savings(self, receiver, amount, memo):
        self._wallet.api.transfer_to_savings(self._name, receiver, amount, memo)

    def transfer_from_savings(self, transfer_id, receiver, amount, memo):
        self._wallet.api.transfer_from_savings(self._name, transfer_id, receiver, amount, memo)

    def cancel_transfer_from_savings(self, transfer_id):
        self._wallet.api.cancel_transfer_from_savings(self._wallet, transfer_id)

    def get_hbd_savings_balance(self) -> tt.Asset.Tbd:
        return tt.Asset.from_(self._node.api.database.find_accounts(accounts=[self._name])[
                                  'accounts'][0]['savings_hbd_balance'])

    def get_hive_savings_balance(self) -> tt.Asset.Test:
        return tt.Asset.from_(self._node.api.database.find_accounts(accounts=[self._name])[
                                  'accounts'][0]['savings_balance'])


@pytest.fixture
def create_account_object():
    return TransferAccount


@pytest.fixture
def alice(prepared_node, wallet, create_account_object):
    wallet.create_account("alice", hives=450, hbds=450, vests=50)
    return create_account_object("alice", prepared_node, wallet)


@pytest.fixture
def bob(prepared_node, wallet, create_account_object):
    wallet.create_account("bob", hives=300, hbds=300, vests=50)
    return create_account_object("bob", prepared_node, wallet)


@pytest.fixture
def carol(prepared_node, wallet, create_account_object):
    wallet.create_account("carol", hives=400, hbds=400, vests=50)
    return create_account_object("carol", prepared_node, wallet)


@pytest.fixture
def daisy(prepared_node, wallet, create_account_object):
    wallet.create_account("daisy", hives=480, hbds=480, vests=50)
    return create_account_object("daisy", prepared_node, wallet)


@pytest.fixture
def elizabeth(prepared_node, wallet, create_account_object):
    wallet.create_account("elizabeth", hives=600, hbds=600, vests=50)
    return create_account_object("elizabeth", prepared_node, wallet)


@pytest.fixture
def hive_fund(prepared_node, wallet, create_account_object):
    return create_account_object("hive.fund", prepared_node, wallet)


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
    speed_up_node.set_vest_price(quote=tt.Asset.Vest(1800), invest=tt.Asset.Test(10_000_000))

    wallet.api.update_witness('initminer', 'http://url.html',
                              tt.Account('initminer').public_key,
                              {'account_creation_fee': tt.Asset.Test(3)}
                              )

    speed_up_node.wait_number_of_blocks(43)

    return speed_up_node
