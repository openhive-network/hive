from __future__ import annotations

import pytest
from dataclasses import dataclass
from typing import Literal, Optional, Union
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


@dataclass
class UpdateAccount(Account):
    __key_generation_counter = 0

    def assert_account_details_were_changed(self, *, new_json_meta: str = None, new_owner: Union[str, list] = None,
                                            new_active: Union[str, list] = None,  new_posting: Union[str, list] = None,
                                            new_memo: str = None):
        # for owner/active/posting argument method accepts both the authority (list - [key, weight]) and key as string
        self.update_account_info()
        if new_owner is not None:
            to_compare = list(self._acc_info["owner"]["key_auths"][0]) if isinstance(new_owner, list) else (
                self._acc_info)["owner"]["key_auths"][0][0]
            assert new_owner == to_compare, f"Owner authority of account {self._name} wasn't changed."

        if new_active is not None:
            to_compare = list(self._acc_info["active"]["key_auths"][0]) if isinstance(new_active, list) else (
                self._acc_info)["active"]["key_auths"][0][0]
            assert new_active == to_compare, f"Active authority of account {self._name} wasn't changed."

        if new_posting is not None:
            to_compare = list(self._acc_info["posting"]["key_auths"][0]) if isinstance(new_posting, list) else (
                self._acc_info)["posting"]["key_auths"][0][0]
            assert new_posting == to_compare, f"Posting authority of account {self._name} wasn't changed."

        if new_memo is not None:
            to_compare = self._acc_info["memo_key"]
            assert new_memo == to_compare, f"Memo key of account {self._name} wasn't changed."

        if new_json_meta is not None:
            to_compare = self._acc_info["json_metadata"]
            assert new_json_meta == to_compare, f"Json metadata of account {self._name} wasn't changed."

    def generate_new_key(self):
        self.__key_generation_counter += 1
        return tt.Account(self._name, secret=f"other_than_previous_{self.__key_generation_counter}").public_key

    def get_current_key(self, type_: str):
        assert type_ in ("owner", "active", "posting", "memo"), "Wrong key type."
        self.update_account_info()
        return self._acc_info[type_]["key_auths"][0][0] if type_ is not "memo" else self._acc_info[type_+"_key"]

    def update_all_account_details(self, *, json_meta: str, owner: str, active: str, posting: str, memo: str):
        self._wallet.api.update_account(self._name, json_meta=json_meta, owner=owner, active=active, posting=posting,
                                        memo=memo, broadcast=True)

    def update_single_account_detail(self, *, json_meta: str = None, key_type: str = None, key: tt.PublicKey = None,
                                     weight: Optional[int] = 1):
        """
        Swapping only one key owner/active/posting require firstly adding new key and then deleting old one
        (setting weight equal to zero). This has to be done because update account requires all 5 arguments. Methods
        changing only one variable like update_account_auth_key, update_account_meta and  update_account_memo_key call
        update_account anyway, so it's still being tested.
        """
        if json_meta is not None:
            self._wallet.api.update_account_meta(self._name, json_meta, broadcast=True)
            return
        assert key_type in ("owner", "active", "posting", "memo"), "Wrong authority type."
        if key_type == "memo":
            self._wallet.api.update_account_memo_key(self._name, key, broadcast=True)
            return
        self.update_account_info()
        current_key = self._acc_info[key_type]["key_auths"][0][0]
        # add new / update weight of existing key
        self._wallet.api.update_account_auth_key(self._name, key_type, key, weight, broadcast=True)
        if current_key != key:
            # generate private key corresponding given public key
            new_private = tt.Account(self._name, secret=f"other_than_previous_{self.__key_generation_counter}").private_key
            # delete old one - make weight equal to zero
            self._wallet.api.update_account_auth_key(self._name, key_type, current_key, 0, broadcast=True)
            self._wallet.api.import_key(new_private)

    def use_authority(self, authority_type: str):
        self._wallet.api.use_authority(authority_type, self._name)


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
