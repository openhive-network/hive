from __future__ import annotations

import json
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any, Literal

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_TREASURY_FEE
from hive_local_tools.functional.python.operation import (
    Account,
    create_transaction_with_any_operation,
    get_transaction_timestamp,
)
from schemas.fields.compound import Authority, HbdExchangeRate
from schemas.operations.account_update2_operation import AccountUpdate2Operation
from schemas.operations.account_update_operation import AccountUpdateOperation
from schemas.operations.limit_order_create2_operation import (
    LimitOrderCreate2OperationLegacy,
)

if TYPE_CHECKING:
    from schemas.fields.basic import PublicKey


class UnknownKeyAuthsFormatError(Exception):
    """Raised if given Key Auths are in invalid format"""


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
            self._node.get_head_block_time() + tt.Time.seconds(expiration),
            format_=tt.TimeFormats.DEFAULT_FORMAT,
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
class ProposalAccount(Account):
    def __post_init__(self) -> None:
        super().__post_init__()
        self.update_account_info()
        self._proposal_parameters = {}

    @property
    def proposal_parameters(self) -> dict:
        return self._proposal_parameters

    def assert_hbd_balance_wasnt_changed(self) -> None:
        old_hbd_balance = self.hbd
        self.update_account_info()
        assert old_hbd_balance == self.hbd, "HBD balance was changed after broadcasting transaction while it shouldn't."

    def create_proposal(self, proposal_receiver: str, start_date: str, end_date: str) -> dict:
        self._proposal_parameters["receiver"] = proposal_receiver
        for parameter_name, value in zip(
            ("receiver", "start_date", "end_date", "daily_pay", "subject", "permlink"),
            (proposal_receiver, start_date, end_date, tt.Asset.Tbd(5), "subject", "comment-permlink"),
        ):
            self._proposal_parameters[parameter_name] = value

        return self._wallet.api.create_proposal(
            self._name,
            self._proposal_parameters["receiver"],
            self._proposal_parameters["start_date"],
            self._proposal_parameters["end_date"],
            self._proposal_parameters["daily_pay"],
            self._proposal_parameters["subject"],
            self._proposal_parameters["permlink"],
        )

    def update_proposal(
        self,
        *,
        proposal_to_update_details: dict | None = None,
        daily_pay: tt.Asset.Tbd = None,
        subject: str | None = None,
        permlink: str | None = None,
        end_date: str | None = None,
    ) -> dict:
        if self._proposal_parameters == {} and proposal_to_update_details is not None:
            self._proposal_parameters = proposal_to_update_details
        new_daily_pay = daily_pay if daily_pay is not None else self._proposal_parameters["daily_pay"]
        new_subject = subject if subject is not None else self._proposal_parameters["subject"]
        new_permlink = permlink if permlink is not None else self._proposal_parameters["permlink"]
        new_end_date = tt.Time.parse(end_date) if end_date is not None else self._proposal_parameters["end_date"]
        for parameter_name, value in zip(
            ("end_date", "daily_pay", "subject", "permlink"), (new_end_date, new_daily_pay, new_subject, new_permlink)
        ):
            self._proposal_parameters[parameter_name] = value
        return self._wallet.api.update_proposal(0, self._name, new_daily_pay, new_subject, new_permlink, new_end_date)

    def remove_proposal(self, proposal_to_remove_details: dict | None = None) -> dict:
        if self._proposal_parameters == {} and proposal_to_remove_details is not None:
            self._proposal_parameters = proposal_to_remove_details
        return self._wallet.api.remove_proposal(self._name, [0])

    def check_if_proposal_was_updated(self, changed_parameter: str) -> None:
        proposal = self._node.api.database.list_proposals(
            start=[""], limit=100, order="by_creator", order_direction="ascending", status="all"
        ).proposals[0]
        assert (
            proposal[changed_parameter] == self._proposal_parameters[changed_parameter]
        ), f"Something went wrong after proposal update. {changed_parameter} has wrong value"

    def check_if_rc_current_mana_was_reduced(self, transaction: dict) -> None:
        self.rc_manabar.assert_rc_current_mana_is_reduced(
            transaction["rc_cost"], get_transaction_timestamp(self._node, transaction)
        )

    def check_if_hive_treasury_fee_was_substracted_from_account(self) -> None:
        # for each day over 60 days HIVE_PROPOSAL_FEE_INCREASE_AMOUNT (1 HBD) is added to fee
        additional_fee = 0
        proposal_duration_days = (
            tt.Time.parse(self._proposal_parameters["end_date"])
            - tt.Time.parse(self._proposal_parameters["start_date"])
        ).days
        if proposal_duration_days > 60:
            additional_fee = proposal_duration_days - 60
        old_balance = self.hbd
        self.update_account_info()
        assert old_balance == self.hbd + tt.Asset.Tbd(
            HIVE_TREASURY_FEE + additional_fee
        ), "HIVE_TREASURY_FEE wasn't substracted from proposal creator account"


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

    @classmethod
    def authority_changed(cls, new_key: str | list | dict[str, Any], old_key: list[tuple[PublicKey, int]]) -> bool:
        old_public_key = old_key[0][0]
        if isinstance(new_key, str):
            return old_public_key == new_key
        if isinstance(new_key, dict):
            return cls.authority_changed(new_key["key_auths"], old_key)
        if isinstance(new_key, list | tuple):
            return cls.authority_changed(new_key[0], old_key)
        raise UnknownKeyAuthsFormatError

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
            assert self.authority_changed(
                new_owner, self._acc_info.owner.key_auths
            ), f"Owner authority of account {self._name} wasn't changed."

        if new_active is not None:
            assert self.authority_changed(
                new_active, self._acc_info.active.key_auths
            ), f"Active authority of account {self._name} wasn't changed."

        if new_posting is not None:
            assert self.authority_changed(
                new_posting, self._acc_info.posting.key_auths
            ), f"Posting authority of account {self._name} wasn't changed."

        if new_memo is not None:
            assert new_memo == self._acc_info.memo_key, f"Memo key of account {self._name} wasn't changed."

        if new_json_meta is not None:
            assert (
                new_json_meta == self._acc_info.json_metadata
            ), f"Json metadata of account {self._name} wasn't changed."

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

    def generate_new_key(self) -> PublicKey:
        self.__key_generation_counter += 1
        return tt.Account(self._name, secret=f"other_than_previous_{self.__key_generation_counter}").public_key

    def get_current_key(self, type_: Literal["owner", "active", "posting", "memo"]) -> str:
        assert type_ in ("owner", "active", "posting", "memo"), "Wrong key type."
        if type_ == "memo":
            type_ = "memo_key"
        self.update_account_info()
        authority: Authority = self._acc_info[type_]
        return authority.key_auths[0][0]

    def update_all_account_details(self, *, json_meta: str, owner: str, active: str, posting: str, memo: str) -> None:
        self._wallet.api.update_account(
            self._name, json_meta=json_meta, owner=owner, active=active, posting=posting, memo=memo, broadcast=True
        )

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
        operation = AccountUpdate2Operation if use_account_update2 else AccountUpdateOperation
        return create_transaction_with_any_operation(self._wallet, operation(account=self.name, **to_pass))

    def update_single_account_detail(
        self,
        *,
        json_meta: str | None = None,
        key_type: str | None = None,
        key: tt.PublicKey = None,
        weight: int | None = 1,
    ) -> dict[str, Any]:
        """
        Swapping only one key owner/active/posting require firstly adding new key and then deleting old one
        (setting weight equal to zero). This has to be done because update account requires all 5 arguments. Methods
        changing only one variable like update_account_auth_key, update_account_meta and  update_account_memo_key call
        update_account anyway, so it's still being tested.
        """
        if json_meta is not None:
            transaction = self._wallet.api.update_account_meta(self._name, json_meta, broadcast=True)
            tt.logger.info(f"json meta RC COST {transaction['rc_cost']}")
            return transaction
        assert key_type in ("owner", "active", "posting", "memo"), "Wrong authority type."
        if key_type == "memo":
            transaction = self._wallet.api.update_account_memo_key(self._name, key, broadcast=True)
            tt.logger.info(f"change memo RC COST {transaction['rc_cost']}")
            return transaction
        self.update_account_info()
        current_key = self._acc_info[key_type]["key_auths"][0][0]
        # add new / update weight of existing key
        transaction = self._wallet.api.update_account_auth_key(self._name, key_type, key, weight, broadcast=True)
        tt.logger.info(f"add new / update weight of existing key RC COST {transaction['rc_cost']}")
        if current_key != key:
            # generate private key corresponding given public key
            new_private = tt.Account(
                self._name, secret=f"other_than_previous_{self.__key_generation_counter}"
            ).private_key
            # delete old one - make weight equal to zero
            transaction = self._wallet.api.update_account_auth_key(self._name, key_type, current_key, 0, broadcast=True)
            tt.logger.info(f"delete old key RC COST {transaction['rc_cost']}")
            self._wallet.api.import_key(new_private)
            return transaction
        return transaction

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
