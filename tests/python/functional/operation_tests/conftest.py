from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Any, Literal

import pytest

import test_tools as tt
import wax
from hive_local_tools.constants import (
    HIVE_100_PERCENT,
    HIVE_COLLATERALIZED_CONVERSION_FEE,
    HIVE_TREASURY_FEE,
)
from hive_local_tools.functional.python.operation import (
    Account,
    create_transaction_with_any_operation,
    get_current_median_history_price,
    get_transaction_timestamp,
    get_virtual_operations,
)
from schemas.fields.compound import Authority, HbdExchangeRate
from schemas.operations.account_update2_operation import AccountUpdate2Operation
from schemas.operations.account_update_operation import AccountUpdateOperation
from schemas.operations.limit_order_create2_operation import (
    LimitOrderCreate2Operation,
)
from schemas.operations.virtual.collateralized_convert_immediate_conversion_operation import (
    CollateralizedConvertImmediateConversionOperation,
)
from schemas.operations.virtual.fill_collateralized_convert_request_operation import (
    FillCollateralizedConvertRequestOperation,
)
from schemas.operations.virtual.fill_convert_request_operation import FillConvertRequestOperation
from schemas.operations.witness_block_approve_operation import WitnessBlockApproveOperation
from schemas.operations.witness_set_properties_operation import WitnessSetPropertiesOperation

if TYPE_CHECKING:
    from schemas.fields.basic import PublicKey
    from schemas.virtual_operation import (
        VirtualOperation as SchemaVirtualOperation,
    )


class UnknownKeyAuthsFormatError(Exception):
    """Raised if given Key Auths are in invalid format"""


@dataclass
class ConvertAccount(Account):
    _added_hbds_by_convert: list = field(default_factory=list, init=False)
    _added_hbds_by_convert_it: int = field(default=-1)

    def __post_init__(self):
        super().__post_init__()
        self.update_account_info()

    @property
    def added_hbds_by_convert(self) -> tt.Asset.TbdT:
        self._added_hbds_by_convert_it += 1
        return self._added_hbds_by_convert[self._added_hbds_by_convert_it]

    def assert_collateralized_conversion_requests(self, trx: dict, state: Literal["create", "delete"]) -> None:
        # extract requestid from transaction
        operations_from_transaction = trx["operations"]
        for operation in operations_from_transaction:
            if operation[0] == "collateralized_convert":
                requestid = operation[1]["requestid"]

        assert "requestid" in locals(), "Provided transaction doesn't contain collateralized convert operation."

        requests = self._node.api.database.list_collateralized_conversion_requests(
            start=[""], limit=100, order="by_account"
        )["requests"]
        all_request_ids = []
        for request in requests:
            all_request_ids.append(request.requestid)

        match state:
            case "create":
                message = "Collateralized conversion request wasn't created."
                assert requestid in all_request_ids, message
            case "delete":
                message = "Collateralized conversion request wasn't deleted after HIVE_CONVERSION_DELAY time."
                assert requestid not in all_request_ids, message

    def assert_fill_collateralized_convert_request_operation(self, expected_amount: int) -> None:
        vops = self.__get_vops_from_more_than_2000_blocks(FillCollateralizedConvertRequestOperation)
        assert (
            len(vops) == expected_amount
        ), "fill_collateralized_convert_request_operation virtual operation wasn't generated"

    def assert_collateralized_convert_immediate_conversion_operation(self, expected_amount: int) -> None:
        vops = self.__get_vops_from_more_than_2000_blocks(CollateralizedConvertImmediateConversionOperation)
        assert (
            len(vops) == expected_amount
        ), "collateralized_convert_immediate_conversion virtual operation wasn't generated"

    def assert_fill_convert_request_operation(self, expected_amount: int) -> None:
        vops = self.__get_vops_from_more_than_2000_blocks(FillConvertRequestOperation)
        assert len(vops) == expected_amount, "fill_convert_request virtual operation wasn't generated"

    def __get_vops_from_more_than_2000_blocks(self, vop: type[SchemaVirtualOperation]) -> list:
        collected_vops = []
        last_block_number = self._node.get_last_block_number()
        end_block = 2000
        for _iterations in range((last_block_number // 2000) + 1):
            vops = get_virtual_operations(self._node, vop, start_block=end_block - 1999, end_block=end_block)
            collected_vops.extend(vop for vop in vops if vop not in collected_vops)
            end_block += 2000
        return collected_vops

    def convert_hbd(self, amount: tt.Asset.Tbd, broadcast: bool = True) -> dict:
        return self._wallet.api.convert_hbd(self._name, amount, broadcast=broadcast)

    def convert_hives(self, amount: tt.Asset.Test, broadcast: bool = True) -> dict:
        return self._wallet.api.convert_hive_with_collateral(self._name, amount, broadcast=broadcast)

    def check_if_right_amount_was_converted_and_added_to_balance(self, transaction: dict) -> None:
        old_hive_balance = self.hive
        self.update_account_info()
        median = get_current_median_history_price(self._node)
        amount_to_convert = self.extract_amount_from_convert_operation(transaction)
        exchange_rate = int(median["quote"]["amount"]) / int(median["base"]["amount"])
        should_be_added = tt.Asset.Test(int(amount_to_convert.amount) / 1000 * exchange_rate)
        tolerance = tt.Asset.Test(0.001)
        assert (
            self.hive - tolerance <= should_be_added + old_hive_balance <= self.hive + tolerance
        ), "Funds were not added to account balance after HBD conversion."

    def check_if_funds_to_convert_were_subtracted(self, transaction: dict) -> None:
        to_convert = self.extract_amount_from_convert_operation(transaction)
        get_balance = self.get_hbd_balance if isinstance(to_convert, tt.Asset.TbdT) else self.get_hive_balance
        funds_before_operation = self.hbd if isinstance(to_convert, tt.Asset.TbdT) else self.hive
        self.update_account_info()
        funds_after_operation = get_balance()
        assert (
            funds_before_operation == funds_after_operation + to_convert
        ), f"Funds weren't subtracted from {self._name} balance after making convert or collateralized convert request."

    def assert_if_right_amount_of_hives_came_back_after_collateral_conversion(self, transaction: dict) -> None:
        to_convert = self.extract_amount_from_convert_operation(transaction)
        current_median = get_current_median_history_price(self._node)
        required_amount = (
            (
                int(self.added_hbds_by_convert.amount)
                * int(current_median["quote"]["amount"])
                * (HIVE_100_PERCENT + HIVE_COLLATERALIZED_CONVERSION_FEE)
            )
            / (int(current_median["base"]["amount"]) * HIVE_100_PERCENT)
        ) * 0.001

        old_hive_balance = self.hive
        self.update_account_info()

        if int(to_convert.amount) * 0.001 - required_amount < 0:
            assert old_hive_balance == self.hive, "All collateral funds were used, but hive balance is not correct."
        else:
            hives_to_add = to_convert - tt.Asset.Test(required_amount)
            tolerance = tt.Asset.Test(0.001)
            assert (
                self.hive - tolerance <= hives_to_add + old_hive_balance <= self.hive + tolerance
            ), f"Balance is incorrect. After HIVE_COLLATERALIZED_CONVERSION_DELAY {self._name}"
            f" should get {hives_to_add.amount} HIVES."

    def assert_account_balance_after_creating_collateral_conversion_request(self, transaction: dict) -> None:
        # it checks if hives were substracted and hbds added immediately after creating request
        to_convert = self.extract_amount_from_convert_operation(transaction)
        hives_before_operation = self.hive
        hbds_before_operation = self.hbd
        self.update_account_info()
        hives_after_operation = self.get_hive_balance()
        hbds_after_operation = self.get_hbd_balance()

        response = self._node.api.wallet_bridge.get_feed_history().current_min_history
        current_min_history = int(response["base"]["amount"]) / int(response["quote"]["amount"])
        hbds_to_add = tt.Asset.Tbd(0.95 * current_min_history * int((to_convert / 2).amount) / 1000)

        assert (
            hives_before_operation == hives_after_operation + to_convert
        ), f"Hives weren't subtracted from {self._name}'s balance after making collateralized convert request."

        hbd_tolerance = tt.Asset.Tbd(0.6)
        assert (
            hbds_after_operation - hbd_tolerance
            <= hbds_before_operation + hbds_to_add
            <= hbds_after_operation + hbd_tolerance
        ), f"Hbds weren't added to {self._name}'s balance after making collateralized convert request."
        self._added_hbds_by_convert.append(hbds_after_operation - hbds_before_operation)

    @staticmethod
    def extract_amount_from_convert_operation(transaction: dict) -> tt.Asset.HiveT | tt.Asset.TbdT:
        ops_in_transaction = transaction["operations"]
        # get amount to convert from transaction
        for operation in ops_in_transaction:
            if operation[0] == "convert" or operation[0] == "collateralized_convert":
                to_convert = operation[1]["amount"]
        assert (
            "to_convert" in locals()
        ), "Convert or collateralized_convert operation wasn't found in given transaction."
        return to_convert


@dataclass
class EscrowAccount(Account):
    def __post_init__(self) -> None:
        super().__post_init__()
        self.update_account_info()

    def check_account_balance(
        self, trx: dict, mode: Literal["escrow_creation", "escrow_rejection", "escrow_release"]
    ) -> None:
        old_hive_amount = self.hive
        old_hbd_amount = self.hbd

        self.update_account_info()
        fee, hbd_amount, hive_amount = self.__extract_escrow_values_from_transaction(trx)

        hbd_message = (
            f"Escrow fee ({fee}) {f'and HBD amount ({hbd_amount})' if hbd_amount != tt.Asset.Tbd(0) else ''} weren't"
            f" {'subtracted from' if mode == 'escrow_creation' else 'added to'} account HBD balance."
        )
        hive_message = (
            f"Escrow HIVE amount ({hive_amount}) wasn't"
            f" {'subtracted from' if mode == 'escrow_creation' else 'added to'} account HIVE balance."
        )
        assert (
            old_hive_amount + hive_amount
            if mode == "escrow_rejection" or mode == "escrow_release"
            else old_hive_amount == self.hive + hive_amount if mode == "escrow_creation" else self.hive
        ), hive_message
        assert (
            old_hbd_amount + (hbd_amount + fee)
            if mode == "escrow_rejection" or mode == "escrow_release"
            else old_hbd_amount == self.hbd + (hbd_amount + fee) if mode == "escrow_creation" else self.hbd
        ), hbd_message

    def check_if_escrow_fee_was_added_to_agent_balance_after_approvals(self, trx: dict) -> None:
        fee = self.__extract_escrow_values_from_transaction(trx)[0]
        old_hbd_balance = self.hbd
        self.update_account_info()
        assert old_hbd_balance + fee == self.hbd, f"Fee ({fee}) wasn't added to agent's balance after escrow approvals."

    @staticmethod
    def __extract_escrow_values_from_transaction(trx: dict) -> tuple | None:
        ops = trx["operations"]
        for op in ops:
            if op[0] == "escrow_transfer" or op[0] == "escrow_release":
                trx_value = op[1]
                fee = trx_value["fee"] if op[0] == "escrow_transfer" else tt.Asset.Tbd(0)
                hbd_amount = trx_value["hbd_amount"]
                hive_amount = trx_value["hive_amount"]
                return fee, hbd_amount, hive_amount

        return None


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
            "cancellation": (
                f"Something went wrong after cancelling an order. {self._name} should have {amount} "
                f"{currency.get_symbol()}."
            ),
            "creation": f"{currency.get_symbol()} balance of {self._name} wasn't reduced after creating order.",
            "expiration": (
                f"Something went wrong after expiration of orders. {self._name} should have"
                f" {amount} {currency.get_symbol()}."
            ),
            "no_match": (
                f"Something went wrong after order creation - it shouldn't be matched. {currency.get_symbol()} balance"
                f" of {self._name} should be equal to {amount}."
            ),
            "order_match": (
                f"Something went wrong in completing a trade. {currency.get_symbol()} balance of {self._name} should be"
                f" equal to {amount}."
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

    def cancel_order(self, *, order_id: int = 0):
        return self._wallet.api.cancel_order(self._name, order_id)

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
    ) -> dict[str, Any]:
        expiration_time = tt.Time.serialize(
            self._node.get_head_block_time() + tt.Time.seconds(expiration),
            format_=tt.TimeFormats.DEFAULT_FORMAT,
        )

        base = tt.Asset.Test if buy_hbd else tt.Asset.Tbd
        quote = tt.Asset.Test if not buy_hbd else tt.Asset.Tbd

        return create_transaction_with_any_operation(
            self._wallet,
            [
                LimitOrderCreate2Operation(
                    owner=self._name,
                    orderid=order_id,
                    amount_to_sell=base(amount_to_sell).as_nai(),
                    exchange_rate=HbdExchangeRate(
                        base=base(amount_to_sell).as_nai(),
                        quote=quote(min_to_receive).as_nai(),
                    ),
                    fill_or_kill=fill_or_kill,
                    expiration=expiration_time,
                )
            ],
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
        self.rc_manabar.assert_current_mana_is_unchanged()

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
        json_metadata: str = "",
        owner: str | None = None,
        active: str | None = None,
        posting: str | None = None,
        memo_key: str | None = None,
        posting_json_metadata: str = "",
    ) -> dict:
        arguments = locals()
        to_pass = {
            element: arguments[element]
            for element in arguments
            if arguments[element] is not None and element not in ("arguments", "self", "use_account_update2")
        }
        if use_account_update2 is False:
            to_pass.pop("posting_json_metadata")
        if memo_key is None and posting_json_metadata == "":
            to_pass["memo_key"] = self._wallet.api.get_account(self.name).memo_key
        operation = AccountUpdate2Operation if use_account_update2 else AccountUpdateOperation
        return create_transaction_with_any_operation(self._wallet, [operation(account=self.name, **to_pass)])

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


class WitnessAccount(Account):
    def __post_init__(self) -> None:
        super().__post_init__()
        self.update_account_info()

    def assert_if_feed_publish_operation_was_generated(self, transaction: dict) -> None:
        operations = self._node.api.account_history.get_transaction(
            id=transaction["transaction_id"], include_reversible=True
        ).operations
        for operation in operations:
            if operation.type_ == "feed_publish_operation" and operation.value.publisher == self._name:
                return
        raise AssertionError("Feed_publish operation wasn't found.")

    def assert_if_rc_current_mana_was_reduced(self, transaction: dict) -> None:
        self.rc_manabar.assert_rc_current_mana_is_reduced(
            transaction["rc_cost"], get_transaction_timestamp(self._node, transaction)
        )
        self.rc_manabar.update()

    def assert_rc_current_mana_was_unchanged(self) -> None:
        self.rc_manabar.assert_current_mana_is_unchanged()

    def become_witness(
        self, url: str, account_creation_fee: tt.Asset.Test, maximum_block_size: int, hbd_interest_rate: int
    ) -> dict:
        self._url = url
        self._account_creation_fee = account_creation_fee
        self._maximum_block_size = maximum_block_size
        self._hbd_interest_rate = hbd_interest_rate
        return self._wallet.api.update_witness(
            self._name,
            self._url,
            tt.Account(self._name).public_key,
            {
                "account_creation_fee": self._account_creation_fee,
                "maximum_block_size": self._maximum_block_size,
                "hbd_interest_rate": self._hbd_interest_rate,
            },
        )

    def check_if_account_has_witness_role(self, expected_witness_role: bool) -> None:
        # block signing key equal to STM1111111111111111111111111111111114T1Anm means that the key is empty - witness
        # is deactivated
        # related issue: https://gitlab.syncad.com/hive/hive/-/issues/681
        empty_key = "STM1111111111111111111111111111111114T1Anm"
        witnesses = self._node.api.database.list_witnesses(start="", limit=100, order="by_name").witnesses
        for witness in witnesses:
            if witness.owner == self._name and witness.signing_key != empty_key:
                found_as_witness = True

        if "found_as_witness" in locals():
            if expected_witness_role:
                return
            raise AssertionError("Witness is listed in list_witnesses, but it shouldn't be.")
        if expected_witness_role:
            raise AssertionError("Witness isn't listed in list_witnesses, but it should be.")

    def feed_publish(self, *, base: int, quote: int, broadcast: bool = True) -> dict:
        exchange_rate = HbdExchangeRate(base=tt.Asset.Tbd(base), quote=tt.Asset.Test(quote))
        return self._wallet.api.publish_feed(self._name, exchange_rate, broadcast=broadcast)

    def resign_from_witness_role(self) -> dict:
        return self._wallet.api.update_witness(
            self._name,
            "http://url.html",
            "STM1111111111111111111111111111111114T1Anm",
            {
                "account_creation_fee": tt.Asset.Test(28),
                "maximum_block_size": 131072,
                "hbd_interest_rate": 1000,
            },
        )

    def update_witness_properties(
        self,
        *,
        new_maximum_block_size: int | None = None,
        new_hbd_interest_rate: int | None = None,
        new_block_signing_key: str | None = None,
        new_url: str | None = None,
        new_account_creation_fee: tt.Asset.TestT = None,
    ) -> dict:
        return self._wallet.api.update_witness(
            self._name,
            self._url if new_url is None else new_url,
            tt.Account(self._name).public_key if new_block_signing_key is None else new_block_signing_key,
            {
                "account_creation_fee": (
                    self._account_creation_fee if new_account_creation_fee is None else new_account_creation_fee
                ),
                "maximum_block_size": (
                    self._maximum_block_size if new_maximum_block_size is None else new_maximum_block_size
                ),
                "hbd_interest_rate": (
                    self._hbd_interest_rate if new_hbd_interest_rate is None else new_hbd_interest_rate
                ),
            },
        )

    def witness_set_properties(self, props_to_serialize: dict) -> dict:
        """
        Following properties can be set in props:
          - account_creation_fee
          - account_subsidy_budget
          - account_subsidy_decay
          - maximum_block_size
          - hbd_interest_rate
          - hbd_exchange_rate
          - url
          - signing_key
          Additionally public key (mandatory for changing any property)
          More details: https://gitlab.syncad.com/hive/hive/-/blob/master/doc/witness_parameters.md?ref_type=heads
        """
        if "account_creation_fee" in props_to_serialize:
            props_to_serialize["account_creation_fee"] = wax.python_json_asset(
                **props_to_serialize["account_creation_fee"]
            )
        if "hbd_exchange_rate" in props_to_serialize:
            props_to_serialize["hbd_exchange_rate"] = wax.python_price(
                base=wax.python_json_asset(**props_to_serialize["hbd_exchange_rate"]["base"]),
                quote=wax.python_json_asset(**props_to_serialize["hbd_exchange_rate"]["quote"]),
            )

        serialized_props = wax.serialize_witness_set_properties(
            wax.python_witness_set_properties_data(**props_to_serialize)
        )
        serialized_props = [[key.decode("utf-8"), value.decode("utf-8")] for key, value in serialized_props.items()]

        return create_transaction_with_any_operation(
            wallet=self._wallet,
            operations=[WitnessSetPropertiesOperation(owner=self._name, props=serialized_props)],
        )

    def witness_block_approve(self, *, block_id: int) -> dict:
        return create_transaction_with_any_operation(
            self._wallet, [WitnessBlockApproveOperation(witness=self._name, block_id=block_id)]
        )


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
    node.run(timeout=60.0, time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
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
