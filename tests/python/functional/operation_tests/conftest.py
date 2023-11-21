from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any, Literal

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    Account,
    create_transaction_with_any_operation,
    get_transaction_timestamp,
)


@dataclass
class LimitOrderAccount(Account):
    def assert_balance(
        self,
        *,
        amount: float,
        check_hbd: bool,
        message: Literal["expiration", "creation", "order_match", "no_match"],
    ) -> None:
        super().update_account_info()
        get_balance = self.get_hbd_balance if check_hbd else self.get_hive_balance
        asset = tt.Asset.Tbd if check_hbd else tt.Asset.Test
        currency = asset(0).get_asset_information()
        messages = {
            "expiration": (
                f"Something went wrong after expiration of orders. {self._name} should have"
                f" {amount} {currency.get_symbol()}."
            ),
            "creation": f"{currency} balance of {self._name} wasn't reduced after creating order.",
            "order_match": (
                f"Something went wrong in completing a trade. {currency.get_symbol()} balance of {self._name} should be"
                f" equal to {amount}."
            ),
            "no_match": (
                f"Something went wrong after order creation - it shouldn't be matched. {currency.get_symbol()} balance"
                f" of {self._name} should be equal to {amount}."
            ),
        }

        assert get_balance() == asset(amount), messages[message]

    def assert_not_completed_order(self, amount: int, hbd: bool) -> None:
        query = self._node.api.database.find_limit_orders(account=self._name).orders[0]
        for_sale = query.for_sale
        nai = query.sell_price.base
        nai = nai.clone(amount=for_sale)
        currency = tt.Asset.Tbd if hbd else tt.Asset.Test
        token = currency(0).get_asset_information().get_symbol()
        assert nai == currency(amount), (
            f"Amount of {token} that are still available for sale "
            f"is not correct. {self._name} should have now {amount} {token}"
        )

    def create_order(
        self,
        amount_to_sell: int,
        min_to_receive: int,
        *,
        order_id: int = 0,
        fill_or_kill: bool = False,
        expiration: int = 60,
        buy_hbd: bool | None = None,
    ) -> dict[str, Any]:
        return self._wallet.api.create_order(
            self._name,
            order_id,
            tt.Asset.Test(amount_to_sell) if buy_hbd else tt.Asset.Tbd(amount_to_sell),
            tt.Asset.Tbd(min_to_receive) if buy_hbd else tt.Asset.Test(min_to_receive),
            fill_or_kill,
            expiration,
        )

    def create_order_2(
        self,
        amount_to_sell: int,
        min_to_receive: int,
        *,
        order_id: int = 0,
        fill_or_kill: bool = False,
        expiration: int = 60,
        buy_hbd: bool | None = None,
    ) -> None:
        expiration_time = tt.Time.serialize(
            self._node.get_head_block_time() + tt.Time.seconds(expiration), format_=tt.Time.DEFAULT_FORMAT
        )

        base = tt.Asset.Test if buy_hbd else tt.Asset.Tbd
        quote = tt.Asset.Test if not buy_hbd else tt.Asset.Tbd

        create_transaction_with_any_operation(
            self._wallet,
            LimitOrderCreate2OperationLegacy(
                owner=self._name,
                order_id=order_id,
                amount_to_sell=base(amount_to_sell).as_legacy(),
                exchange_rate=HbdExchangeRate(
                    base=base(amount_to_sell).as_legacy(),
                    quote=quote(min_to_receive).as_legacy(),
                ),
                fill_or_kill=fill_or_kill,
                expiration=expiration_time,
            ),
        )


@dataclass
class TransferAccount(Account):
    # class with account representation created for transfer, transfer_to/from_savings tests
    def transfer(self, receiver: str, amount: tt.Asset.TestT | tt.Asset.TbdT, memo: str) -> None:
        self._wallet.api.transfer(self._name, receiver, amount, memo)

    def transfer_to_savings(self, receiver: str, amount: tt.Asset.TestT | tt.Asset.TbdT, memo: str) -> None:
        self._wallet.api.transfer_to_savings(self._name, receiver, amount, memo)

    def transfer_from_savings(
        self,
        transfer_id: int,
        receiver: str,
        amount: tt.Asset.TestT | tt.Asset.TbdT,
        memo: str,
    ) -> None:
        self._wallet.api.transfer_from_savings(self._name, transfer_id, receiver, amount, memo)

    def cancel_transfer_from_savings(self, transfer_id: int) -> None:
        self._wallet.api.cancel_transfer_from_savings(self._wallet, transfer_id)

    def get_hbd_savings_balance(self) -> tt.Asset.TbdT:
        return self._node.api.database.find_accounts(accounts=[self._name]).accounts[0].savings_hbd_balance

    def get_hive_savings_balance(self) -> tt.Asset.TestT:
        return self._node.api.database.find_accounts(accounts=[self._name]).accounts[0].savings_balance


@dataclass
class UpdateAccount(Account):
    __key_generation_counter = 0

    def assert_account_details_were_changed(
        self,
        *,
        new_json_meta: str | None = None,
        new_owner: str | list | None = None,
        new_active: str | list | None = None,
        new_posting: str | list | None = None,
        new_memo: str | None = None,
        new_posting_json_meta: str | None = None,
    ):
        self.update_account_info()
        if new_owner is not None:
            assert new_owner == self._acc_info["owner"], f"Owner authority of account {self._name} wasn't changed."

        if new_active is not None:
            assert new_active == self._acc_info["active"], f"Active authority of account {self._name} wasn't changed."

        if new_posting is not None:
            assert (
                new_posting == self._acc_info["posting"]
            ), f"Posting authority of account {self._name} wasn't changed."

        if new_memo is not None:
            assert new_memo == self._acc_info["memo_key"], f"Memo key of account {self._name} wasn't changed."

        if new_json_meta is not None:
            to_compare = self._acc_info["json_metadata"]
            assert new_json_meta == to_compare, f"Json metadata of account {self._name} wasn't changed."

        if new_posting_json_meta is not None:
            to_compare = self._acc_info["posting_json_metadata"]
            assert (
                json.loads(new_posting_json_meta) == to_compare
            ), f"Posting json metadata of account {self._name} wasn't changed."

    def assert_if_rc_current_mana_was_reduced(self, transaction):
        self.rc_manabar.assert_rc_current_mana_is_reduced(
            transaction["rc_cost"], get_transaction_timestamp(self._node, transaction)
        )

    def assert_if_rc_current_mana_was_unchanged(self):
        self.rc_manabar.assert_rc_current_mana_is_unchanged()

    def generate_new_authority(self) -> dict:
        return {"account_auths": [], "key_auths": [(self.generate_new_key(), 1)], "weight_threshold": 1}

    def generate_new_key(self):
        self.__key_generation_counter += 1
        return tt.Account(self._name, secret=f"other_than_previous_{self.__key_generation_counter}").public_key

    def get_current_key(self, type_: str):
        assert type_ in ("owner", "active", "posting", "memo"), "Wrong key type."
        self.update_account_info()
        return self._acc_info[type_]["key_auths"][0][0] if type_ != "memo" else self._acc_info[type_ + "_key"]

    def update_account(
        self,
        *,
        use_account_update2: bool = False,
        json_metadata: str | None = None,
        owner: str | None = None,
        active: str | None = None,
        posting: str | None = None,
        memo_key: str | None = None,
        posting_json_metadata: str | None = None,
    ) -> dict:
        arguments = locals()
        to_pass = {
            element: arguments[element]
            for element in arguments
            if arguments[element] is not None and element not in ("arguments", "self", "use_account_update2")
        }
        operation_name = "account_update2" if use_account_update2 else "account_update"
        return create_transaction_with_any_operation(self._wallet, operation_name, account=self._name, **to_pass)

    def use_authority(self, authority_type: str):
        self._wallet.api.use_authority(authority_type, self._name)


@pytest.fixture()
def create_account_object() -> type[TransferAccount]:
    return TransferAccount


@pytest.fixture()
def alice(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_account_object: type[TransferAccount],
) -> TransferAccount:
    wallet.create_account("alice", hives=450, hbds=450, vests=50)
    return create_account_object("alice", prepared_node, wallet)


@pytest.fixture()
def bob(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_account_object: type[TransferAccount],
) -> TransferAccount:
    wallet.create_account("bob", hives=300, hbds=300, vests=50)
    return create_account_object("bob", prepared_node, wallet)


@pytest.fixture()
def carol(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_account_object: type[TransferAccount],
) -> TransferAccount:
    wallet.create_account("carol", hives=400, hbds=400, vests=50)
    return create_account_object("carol", prepared_node, wallet)


@pytest.fixture()
def daisy(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_account_object: type[TransferAccount],
) -> TransferAccount:
    wallet.create_account("daisy", hives=480, hbds=480, vests=50)
    return create_account_object("daisy", prepared_node, wallet)


@pytest.fixture()
def elizabeth(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_account_object: type[TransferAccount],
) -> TransferAccount:
    wallet.create_account("elizabeth", hives=600, hbds=600, vests=50)
    return create_account_object("elizabeth", prepared_node, wallet)


@pytest.fixture()
def hive_fund(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_account_object: type[TransferAccount],
) -> TransferAccount:
    return create_account_object("hive.fund", prepared_node, wallet)


@pytest.fixture()
def speed_up_node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.run(time_offset="+0h x5")
    return node


@pytest.fixture()
def wallet(speed_up_node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=speed_up_node)


@pytest.fixture()
def prepared_node(speed_up_node: tt.InitNode, wallet: tt.Wallet) -> tt.InitNode:
    speed_up_node.set_vest_price(quote=tt.Asset.Vest(1800), invest=tt.Asset.Test(10_000_000))

    wallet.api.update_witness(
        "initminer", "http://url.html", tt.Account("initminer").public_key, {"account_creation_fee": tt.Asset.Test(3)}
    )

    speed_up_node.wait_number_of_blocks(43)

    return speed_up_node
