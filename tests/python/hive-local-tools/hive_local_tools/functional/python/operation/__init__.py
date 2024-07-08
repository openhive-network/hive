from __future__ import annotations

import json
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from typing import TYPE_CHECKING, Any, Literal

from helpy import wax

import test_tools as tt
from hive_local_tools.constants import (
    HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS,
    get_transaction_model,
)
from schemas.apis.database_api.fundaments_of_reponses import AccountItemFundament
from schemas.fields.compound import Manabar
from schemas.filter import (
    build_vop_filter,
)
from schemas.operations.virtual.fill_transfer_from_savings_operation import FillTransferFromSavingsOperation
from schemas.operations.virtual.transfer_to_vesting_completed_operation import (
    TransferToVestingCompletedOperation,
)

if TYPE_CHECKING:
    from schemas.apis.account_history_api.response_schemas import EnumVirtualOps
    from schemas.operations import AnyLegacyOperation
    from schemas.virtual_operation import (
        VirtualOperation as SchemaVirtualOperation,
    )
from schemas.operations.custom_json_operation import CustomJsonOperation


@dataclass
class Operation:
    _node: tt.InitNode
    _wallet: tt.Wallet
    _rc_cost: int = field(init=False, default=None)

    @property
    def rc_cost(self) -> int:
        return self._rc_cost

    def assert_minimal_operation_rc_cost(self) -> None:
        assert self._rc_cost > 0, "RC cost is less than or equal to zero."


ApiAccountItem = AccountItemFundament[tt.Asset.TestT, tt.Asset.TbdT, tt.Asset.VestsT]


def _find_account(node: tt.InitNode, account_name: str) -> ApiAccountItem:
    return node.api.database.find_accounts(accounts=[account_name]).accounts[0]


@dataclass
class Account:
    _name: str
    _node: tt.InitNode
    _wallet: tt.Wallet
    _acc_info: ApiAccountItem = field(init=False)
    _rc_manabar: _RcManabar = field(init=False)

    def __post_init__(self):
        if self._name:
            self._rc_manabar = _RcManabar(self._node, self._name)
            self._vote_manabar = _VoteManabar(self._node, self._wallet, self._name)
            self._downvote_manabar = _DownvoteManabar(self._node, self._wallet, self._name)

    @property
    def node(self) -> tt.InitNode:
        return self._node

    @property
    def name(self) -> str:
        return self._name

    @property
    def hive(self) -> tt.Asset.TestT:
        return self._acc_info.balance

    @property
    def hbd(self) -> tt.Asset.TbdT:
        return self._acc_info.hbd_balance

    @property
    def rc_manabar(self) -> _RcManabar:
        return self._rc_manabar

    @property
    def vote_manabar(self) -> _VoteManabar:
        return self._vote_manabar

    @property
    def downvote_manabar(self) -> _DownvoteManabar:
        return self._downvote_manabar

    @property
    def vest(self) -> tt.Asset.VestsT:
        return self._acc_info.vesting_shares

    @property
    def reward_hive(self) -> tt.Asset.TestT:
        return self._acc_info.reward_hive_balance

    @property
    def reward_hbd(self) -> tt.Asset.TbdT:
        return self._acc_info.reward_hbd_balance

    @property
    def reward_vests(self) -> tt.Asset.VestsT:
        return self._acc_info.reward_vesting_balance

    @property
    def proxy(self) -> str:
        return self._acc_info.proxy

    def get_governance_vote_power(self, current: bool = False) -> tt.Asset.Vest:
        return self.get_direct_governance_vote_power(current) + tt.Asset.Vest(
            sum(self._acc_info.proxied_vsf_votes) / 1_000_000
        )

    def get_direct_governance_vote_power(self, current: bool) -> tt.Asset.Vest:
        acc_info = _find_account(self.node, self._name) if current else self._acc_info
        return tt.Asset.from_nai(
            {
                "amount": str(int(acc_info.vesting_shares.amount) - sum([vote.val for vote in acc_info.delayed_votes])),
                "precision": 6,
                "nai": "@@000000037",
            }
        )

    def fund_vests(self, tests: tt.Asset.Test) -> None:
        self._wallet.api.transfer_to_vesting(
            "initminer",
            self._name,
            tests,
        )
        self.update_account_info()

    def get_hbd_balance(self) -> tt.Asset.TbdT:
        return get_hbd_balance(self._node, self._name)

    def get_reward_hbd_balance(self) -> tt.Asset.TbdT:
        return get_reward_hbd_balance(self._node, self._name)

    def get_vesting_shares(self) -> tt.Asset.VestsT:
        return get_vesting_shares(self._node, self._name)

    def get_reward_vesting_balance(self) -> tt.Asset.VestsT:
        return get_reward_vesting_balance(self._node, self._name)

    def get_hive_balance(self) -> tt.Asset.TestT:
        return get_hive_balance(self._node, self._name)

    def get_reward_hive_balance(self) -> tt.Asset.TestT:
        return get_reward_hive_balance(self._node, self._name)

    def get_hive_power(self) -> tt.Asset.VestsT:
        return get_hive_power(self._node, self._name)

    def get_rc_current_mana(self) -> int:
        return get_rc_current_mana(self._node, self._name)

    def get_rc_max_mana(self) -> int:
        return get_rc_max_mana(self._node, self._name)

    def get_proxy(self) -> str:
        return get_proxy(self._node, self._name)

    def update_account_info(self) -> None:
        self._acc_info = _find_account(self._node, self._name)
        self._rc_manabar.update()
        self._vote_manabar.update()
        self._downvote_manabar.update()

    def top_up(self, amount: tt.Asset.TestT | tt.Asset.TbdT) -> None:
        self._wallet.api.transfer("initminer", self._name, amount, "{}")
        self.update_account_info()

    def unlock_delayed_votes(self) -> None:
        delayed_votes = self._node.api.database.find_accounts(accounts=[self._acc_info.name]).accounts[0].delayed_votes
        if len(delayed_votes) == 0:
            return

        last_unlock_date = max(
            [
                tt.Time.parse(delay_vote["time"])
                for delay_vote in self._node.api.database.find_accounts(accounts=[self._acc_info.name])
                .accounts[0]
                .delayed_votes
            ]
        )
        self._node.restart(
            time_control=tt.StartTimeControl(
                start_time=last_unlock_date + tt.Time.seconds(HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS)
            )
        )
        assert len(self._node.api.database.find_accounts(accounts=[self.name]).accounts[0].delayed_votes) == 0
        self._acc_info.delayed_votes.clear()

    def check_if_current_rc_mana_was_reduced(self, trx) -> None:
        self.rc_manabar.assert_rc_current_mana_is_reduced(trx["rc_cost"], get_transaction_timestamp(self._node, trx))
        self.rc_manabar.update()

    def check_if_rc_mana_was_unchanged(self) -> None:
        self.rc_manabar.assert_current_mana_is_unchanged()


class _BaseManabar(ABC):
    def __init__(self, node: tt.InitNode, name: str):
        self._node = node
        self._name = name
        self.__manabar: ExtendedManabar | None = None
        self.update()

    @property
    def manabar(self) -> ExtendedManabar:
        assert self.__manabar is not None
        return self.__manabar

    @property
    def current_mana(self) -> int:
        return self.manabar.current_mana

    @property
    def last_update_time(self) -> int:
        return self.manabar.last_update_time

    @property
    def max_mana(self) -> int:
        return self.manabar.maximum

    @abstractmethod
    def _get_specific_manabar(self) -> ExtendedManabar:
        """This method should return current state of derived manabar."""

    @abstractmethod
    def _get_specific_current_mana(self) -> int:
        """This method should return current state of derived current mana."""

    def __str__(self):
        return (
            f"max_mana: {self.max_mana}, current_mana: {self.current_mana}, last_update_time:"
            f" {datetime.fromtimestamp(self.last_update_time)}"
        )

    def calculate_current_value(self, head_block_time: datetime) -> int:
        return int(
            wax.calculate_current_manabar_value(
                now=int(head_block_time.timestamp()),
                max_mana=self.max_mana,
                current_mana=self.current_mana,
                last_update_time=self.last_update_time,
            )
        )

    def update(self) -> None:
        self.__manabar = self._get_specific_manabar()


class _RcManabar(_BaseManabar):
    def __init__(self, node, name):
        super().__init__(node, name)

    def _get_specific_manabar(self) -> ExtendedManabar:
        return get_rc_manabar(self._node, self._name)

    def _get_specific_current_mana(self) -> int:
        return get_rc_current_mana(self._node, self._name)

    def assert_rc_current_mana_is_reduced(
        self, operation_rc_cost: int, operation_timestamp: datetime | None = None
    ) -> None:
        err = f"The account {self._name} did not incur the operation cost."
        if operation_timestamp:
            mana_before_operation = self.calculate_current_value(operation_timestamp - tt.Time.seconds(3))
            assert mana_before_operation == get_rc_current_mana(self._node, self._name) + operation_rc_cost, err
        else:
            assert get_rc_current_mana(self._node, self._name) + operation_rc_cost == self.current_mana, err

    def assert_max_rc_mana_state(self, state: Literal["reduced", "unchanged", "increased"]) -> None:
        actual_max_rc_mana = get_rc_max_mana(self._node, self._name)
        error_message = f"The {self._name} account `rc_max_mana` has been not {state}"
        match state:
            case "unchanged":
                assert actual_max_rc_mana == self.max_mana, error_message
            case "reduced":
                assert actual_max_rc_mana < self.max_mana, error_message
            case "increased":
                assert actual_max_rc_mana > self.max_mana, error_message

    def assert_current_mana_is_unchanged(self) -> None:
        assert (
            get_rc_current_mana(self._node, self._name) == self.current_mana
        ), f"The {self._name} account rc_current_mana has been changed."


class _VoteManabarBase(_BaseManabar):
    def __init__(self, node, wallet: tt.Wallet, name, manabar_type: str):
        self._wallet = wallet
        self.__manabar_type = manabar_type
        super().__init__(node, name)

    def _get_specific_manabar(self) -> ExtendedManabar:
        return get_vote_manabar(self._wallet, self._name, self.__manabar_type)

    def _get_specific_current_mana(self) -> int:
        return get_vote_current_mana(self._wallet, self._name, self.__manabar_type)

    def assert_current_mana_is_reduced(self, operation_timestamp=None) -> None:
        err = f"The account {self._name} did not incur the operation cost."
        vote_current_mana = self._get_specific_current_mana()
        if operation_timestamp:
            mana_before_operation = self.calculate_current_value(operation_timestamp - tt.Time.seconds(3))
            assert vote_current_mana < mana_before_operation, err
        else:
            assert vote_current_mana < self.current_mana, err

    def assert_current_mana_is_unchanged(self) -> None:
        assert (
            get_vote_current_mana(self._wallet, self._name, self.__manabar_type) == self.current_mana
        ), f"The {self._name} account {self.__manabar_type} has been changed."


class _VoteManabar(_VoteManabarBase):
    def __init__(self, node, wallet, name):
        super().__init__(node, wallet, name, "voting_manabar")


class _DownvoteManabar(_VoteManabarBase):
    def __init__(self, node, wallet, name):
        super().__init__(node, wallet, name, "downvote_manabar")


def check_if_fill_transfer_from_savings_vop_was_generated(node: tt.InitNode, memo: str) -> bool:
    payout_vops = get_virtual_operations(node, FillTransferFromSavingsOperation)
    return any(vop["op"]["value"]["memo"] == memo for vop in payout_vops)


def create_account_with_different_keys(wallet: tt.Wallet, account_name: str, creator: str) -> None:
    wallet.api.create_account_with_keys(
        creator,
        account_name,
        "{}",
        tt.Account(account_name, secret="owner_key").public_key,
        tt.Account(account_name, secret="active_key").public_key,
        tt.Account(account_name, secret="posting_key").public_key,
        tt.Account(account_name, secret="memo_key").public_key,
        broadcast=True,
    )

    wallet.api.import_keys(
        [
            tt.Account(account_name, secret="owner_key").private_key,
            tt.Account(account_name, secret="active_key").private_key,
            tt.Account(account_name, secret="posting_key").private_key,
            tt.Account(account_name, secret="memo_key").private_key,
        ]
    )


def create_transaction_with_any_operation(
    wallet: tt.Wallet, *operations: AnyLegacyOperation, only_result: bool = True, broadcast: bool = True
) -> dict[str, Any]:
    # function creates transaction manually because some operations are not added to wallet
    transaction = get_transaction_model()
    ops = []
    for op in operations:
        if isinstance(op, CustomJsonOperation):
            op.json_ = json.dumps(op.json_)
        ops.append((op.get_name(), op))
    transaction.operations = ops
    transaction.operations = [(op.get_name(), op) for op in operations]
    return wallet.api.sign_transaction(transaction, only_result=only_result, broadcast=broadcast)


def get_governance_voting_power(node: tt.InitNode, wallet: tt.Wallet, account_name: str) -> int:
    try:
        wallet.api.set_voting_proxy(account_name, "initminer")
    except tt.exceptions.CommunicationError as error:
        if "Proxy must change" not in str(error):
            raise

    return int(_find_account(node, "initminer").proxied_vsf_votes[0])


def get_hbd_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TbdT:
    return _find_account(node, account_name).hbd_balance


def get_reward_hbd_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TbdT:
    return _find_account(node, account_name).reward_hbd_balance


def get_vesting_shares(node: tt.InitNode, account_name: str) -> tt.Asset.VestsT:
    return _find_account(node, account_name).vesting_shares


def get_reward_vesting_balance(node: tt.InitNode, account_name: str) -> tt.Asset.VestsT:
    return _find_account(node, account_name).reward_vesting_balance


def get_hbd_savings_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TbdT:
    return _find_account(node, account_name).savings_hbd_balance


def get_hive_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TestT:
    return _find_account(node, account_name).balance


def get_reward_hive_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TestT:
    return _find_account(node, account_name).reward_hive_balance


def get_hive_power(node: tt.InitNode, account_name: str) -> tt.Asset.VestsT:
    return _find_account(node, account_name).vesting_shares


def get_current_median_history_price(node: tt.InitNode) -> dict:
    return node.api.wallet_bridge.get_current_median_history_price()


def get_rc_current_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name]).rc_accounts[0].rc_manabar.current_mana)


def get_vote_current_mana(
    wallet: tt.Wallet, account_name: str, bar_type: Literal["voting_manabar", "downvote_manabar"]
) -> int:
    return int(wallet.api.get_account(account_name)[bar_type]["current_mana"])


def get_number_of_fill_order_operations(node: tt.InitNode) -> int:
    return len(
        node.api.account_history.enum_virtual_ops(
            block_range_begin=1,
            block_range_end=node.get_last_block_number() + 1,
            include_reversible=True,
            filter=0x000080,
            group_by_block=False,
        ).ops
    )


def get_number_of_active_proposals(node):
    return len(
        node.api.database.list_proposals(
            start=[""], limit=100, order="by_creator", order_direction="ascending", status="all"
        ).proposals
    )


def get_vesting_price(node: tt.InitNode) -> int:
    """
    Current exchange rate - `1` Hive to Vest conversion price.
    """
    dgpo = node.api.wallet_bridge.get_dynamic_global_properties()
    total_vesting_shares = dgpo.total_vesting_shares.amount
    total_vesting_fund_hive = dgpo.total_vesting_fund_hive.amount

    return int(total_vesting_shares) // int(total_vesting_fund_hive)


def convert_hive_to_vest_range(hive_amount: tt.Asset.TestT, price: float, tolerance: int = 5) -> tt.Asset.Range:
    """
     Converts Hive to VEST resources at the current exchange rate provided in the `price` parameter.
    :param hive_amount: The amount of Hive resources to convert.
    :param price: The current exchange rate for the conversion.
    :param tolerance: The tolerance percent for the VEST conversion, defaults its 5%.
    :return: The equivalent amount of VEST resources after the conversion, within the specified tolerance.
    """
    vests = tt.Asset.VestT(amount=(int(hive_amount.amount) * price))
    return tt.Asset.Range(vests, tolerance=tolerance)


def get_virtual_operations(
    node: tt.InitNode,
    *vops: type[SchemaVirtualOperation],
    skip_price_stabilization: bool = True,
    start_block: int | None = None,
    end_block: int = 2000,
) -> list:
    """
    :param vop: name of the virtual operation,
    :param skip_price_stabilization: by default, removes from the list operations confirming vesting-stabilizing,
    :param start_block: block from which virtual operations will be given,
    :param end_block: block to which virtual operations will be given,
    :return: a list of virtual operations of the type specified in the `vop` argument.
    """
    result: EnumVirtualOps = node.api.account_history.enum_virtual_ops(
        filter=build_vop_filter(*vops),
        include_reversible=True,
        block_range_begin=start_block,
        block_range_end=end_block,
    )

    if skip_price_stabilization:
        for vop_number, vop in enumerate(result.ops):
            if isinstance(
                vop.op.value, TransferToVestingCompletedOperation
            ) and vop.op.value.hive_vested == tt.Asset.Test(10_000_000):
                result.ops.pop(vop_number)
    return result.ops


def get_rc_max_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name])["rc_accounts"][0]["max_rc"])


def get_transaction_timestamp(node: tt.InitNode, transaction) -> datetime:
    return tt.Time.parse(node.api.block.get_block(block_num=transaction["block_num"])["block"]["timestamp"])


def jump_to_date(node: tt.InitNode, time_control: datetime, wait_for_irreversible: bool = False) -> None:
    if wait_for_irreversible:
        node.wait_for_irreversible_block()
    node.restart(time_control=tt.StartTimeControl(start_time=time_control))


class ExtendedManabar(Manabar):
    maximum: int


def get_rc_manabar(node: tt.InitNode, account_name: str) -> ExtendedManabar:
    response = node.api.rc.find_rc_accounts(accounts=[account_name]).rc_accounts[0]
    return ExtendedManabar(
        current_mana=response.rc_manabar.current_mana,
        last_update_time=response.rc_manabar.last_update_time,
        maximum=response.max_rc,
    )


def get_proxy(node: tt.InitNode, account_name: str) -> str:
    return node.api.database.find_accounts(accounts=[account_name]).accounts[0].proxy


def list_votes_for_all_proposals(node):
    return node.api.database.list_proposal_votes(
        start=[""], limit=1000, order="by_voter_proposal", order_direction="ascending", status="all"
    )["proposal_votes"]


def get_vote_manabar(
    wallet: tt.Wallet, account_name: str, bar_type: Literal["voting_manabar", "downvote_manabar"]
) -> ExtendedManabar:
    response = wallet.api.get_account(account_name)
    max_mana = int(tt.Asset.from_legacy(response["post_voting_power"]).amount)
    return ExtendedManabar(
        current_mana=int(response[bar_type]["current_mana"]),
        last_update_time=response[bar_type]["last_update_time"],
        maximum=max_mana if bar_type == "voting_manabar" else int(0.25 * max_mana),
    )


class Proposal:
    def __init__(self, node: tt.InitNode, proposal_id: int) -> None:
        self._node = node
        self._proposal_info = node.api.database.list_proposals(
            start=[""],
            limit=1,
            order="by_start_date",
            order_direction="ascending",
            status="all",
            last_id=proposal_id,
        ).proposals[0]
        self.id = self._proposal_info["id"]
        self.proposal_id = self._proposal_info["proposal_id"]
        self.creator = self._proposal_info["creator"]
        self.receiver = self._proposal_info["receiver"]
        self.start_date = self._proposal_info["start_date"]
        self.end_date = self._proposal_info["end_date"]
        self.daily_pay = self._proposal_info["daily_pay"]
        self.subject = self._proposal_info["subject"]
        self.permlink = self._proposal_info["permlink"]
        self.total_votes = tt.Asset.Vest(self._proposal_info["total_votes"] / 1_000_000)
        self.status = self._proposal_info["status"]

    def update_proposal_info(self):
        self.__init__(self._node, self.proposal_id)


def publish_feeds(node: tt.InitNode, wallet: tt.Wallet, base: int, quote: int, broadcast: bool = True) -> dict:
    response = node.api.database.list_witnesses(start=None, limit=100, order="by_name").witnesses
    witnesses = [element["owner"] for element in response]
    with wallet.in_single_transaction(broadcast=broadcast) as transaction:
        for witness in witnesses:
            exchange_rate = {"base": tt.Asset.Tbd(base), "quote": tt.Asset.Test(quote)}
            wallet.api.publish_feed(witness, exchange_rate, broadcast=False)
    return transaction.get_response()


def publish_feeds_with_confirmation(node: tt.InitNode, wallet: tt.Wallet, base: int, quote: int) -> None:
    old_feed_current_median_history = node.api.wallet_bridge.get_feed_history().current_median_history
    for _ in range(45):
        publish_feeds(node, wallet, base, quote)
        if node.api.wallet_bridge.get_feed_history().current_median_history != old_feed_current_median_history:
            return
    raise TimeoutError("Current median history not change on time")


def convert_from_mainnet_to_testnet_asset(asset: tt.Asset.AnyT) -> tt.Asset.AnyT:
    asset = asset.as_nai()
    return tt.Asset.from_nai(asset).as_legacy()
