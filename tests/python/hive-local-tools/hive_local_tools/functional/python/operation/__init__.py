from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from typing import TYPE_CHECKING, Any, Literal

import test_tools as tt
import wax
from hive_local_tools.constants import (
    HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS,
    get_transaction_model,
)
from schemas.apis.database_api.fundaments_of_reponses import AccountItemFundament
from schemas.fields.compound import Manabar
from schemas.filter import (
    build_vop_filter,
)
from schemas.jsonrpc import get_response_model
from schemas.operations import (
    CommentOperation,
    CommentOptionsOperationLegacy,
    DeleteCommentOperation,
)
from schemas.operations.representations.legacy_representation import LegacyRepresentation  # noqa: TCH001
from schemas.operations.virtual.author_reward_operation import AuthorRewardOperation
from schemas.operations.virtual.comment_benefactor_reward_operation import CommentBenefactorRewardOperation
from schemas.operations.virtual.curation_reward_operation import CurationRewardOperation
from schemas.operations.virtual.effective_comment_vote_operation import EffectiveCommentVoteOperation
from schemas.operations.virtual.fill_transfer_from_savings_operation import FillTransferFromSavingsOperation
from schemas.operations.virtual.transfer_to_vesting_completed_operation import (
    TransferToVestingCompletedOperation,
)
from schemas.transaction import TransactionLegacy

if TYPE_CHECKING:
    from schemas.apis.account_history_api.response_schemas import EnumVirtualOps
    from schemas.fields.hive_int import HiveInt
    from schemas.operations import AnyLegacyOperation
    from schemas.virtual_operation import (
        VirtualOperation as SchemaVirtualOperation,
    )


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
    def proxy(self) -> str:
        return self._acc_info.proxy

    def get_governance_vote_power(self) -> tt.Asset.Vest:
        return self.get_direct_governance_vote_power() + tt.Asset.Vest(
            sum(self._acc_info.proxied_vsf_votes) / 1_000_000
        )

    def get_direct_governance_vote_power(self) -> tt.Asset.Vest:
        return tt.Asset.from_nai(
            {
                "amount": str(
                    int(self._acc_info.vesting_shares.amount) - sum([vote.val for vote in self._acc_info.delayed_votes])
                ),
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

    def get_reward_vesting_balance(self) -> tt.Asset.VestsT:
        return get_reward_vesting_balance(self._node, self._name)

    def get_hive_balance(self) -> tt.Asset.TestT:
        return get_hive_balance(self._node, self._name)

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
            time_offset=tt.Time.serialize(
                last_unlock_date + tt.Time.seconds(HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS),
                format_=tt.TimeFormats.TIME_OFFSET_FORMAT,
            )
        )
        assert len(self._node.api.database.find_accounts(accounts=[self.name]).accounts[0].delayed_votes) == 0
        self._acc_info.delayed_votes.clear()


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
    def last_update_time(self) -> datetime:
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
            ).result
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


def create_transaction_with_any_operation(
    wallet: tt.Wallet, *operations: AnyLegacyOperation, only_result: bool = True
) -> dict[str, Any]:
    # function creates transaction manually because some operations are not added to wallet
    transaction = get_transaction_model()
    transaction.operations = [(op.get_name(), op) for op in operations]
    return wallet.api.sign_transaction(transaction, only_result=only_result)


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


def get_reward_vesting_balance(node: tt.InitNode, account_name: str) -> tt.Asset.VestsT:
    return _find_account(node, account_name).reward_vesting_balance


def get_hbd_savings_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TbdT:
    return _find_account(node, account_name).savings_hbd_balance


def get_hive_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TestT:
    return _find_account(node, account_name).balance


def get_hive_power(node: tt.InitNode, account_name: str) -> tt.Asset.VestsT:
    return _find_account(node, account_name).vesting_shares


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


def jump_to_date(node: tt.InitNode, time_offset: datetime, wait_for_irreversible: bool = False) -> None:
    if wait_for_irreversible:
        node.wait_for_irreversible_block()
    node.restart(
        time_offset=tt.Time.serialize(
            time_offset,
            format_=tt.TimeFormats.TIME_OFFSET_FORMAT,
        )
    )


class ExtendedManabar(Manabar):
    maximum: int


def get_rc_manabar(node: tt.InitNode, account_name: str) -> ExtendedManabar:
    response = node.api.rc.find_rc_accounts(accounts=[account_name]).rc_accounts[0]
    return ExtendedManabar(
        current_mana=response.rc_manabar.current_mana,
        last_update_time=response.rc_manabar.last_update_time,
        maximum=response.max_rc,
    )


class CommentTransaction(TransactionLegacy):
    operations: tuple[LegacyRepresentation[CommentOperation]] | tuple[
        LegacyRepresentation[CommentOperation], LegacyRepresentation[CommentOptionsOperationLegacy]
    ]
    rc_cost: int = 0


class CommentOptionsTransaction(TransactionLegacy):
    operations: tuple[LegacyRepresentation[CommentOptionsOperationLegacy]]
    rc_cost: int = 0


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


def get_reward_operations(node, mode: Literal["author", "curation", "comment_benefactor"]):
    if mode == "author":
        reward_operations = get_virtual_operations(node, AuthorRewardOperation)
    elif mode == "curation":
        reward_operations = get_virtual_operations(node, CurationRewardOperation)
    elif mode == "comment_benefactor":
        reward_operations = get_virtual_operations(node, CommentBenefactorRewardOperation)
    else:
        raise ValueError(f"Unexpected value for 'mode': '{mode}'")
    return [vop["op"]["value"] for vop in reward_operations]


def convert_hbd_to_hive(node, hbd: tt.Asset.Hbd) -> tt.Asset.Hive:
    hbd_to_hive_feed = node.api.database.get_current_price_feed()
    hbd_to_hive_feed = hbd_to_hive_feed["base"].as_float() / hbd_to_hive_feed["quote"].as_float()
    return tt.Asset.Hive(hbd.as_float() / 1000 * hbd_to_hive_feed)


def convert_vesting_to_hive(node, vesting: tt.Asset.Vest) -> tt.Asset.Hive:
    gdgp = node.api.database.get_dynamic_global_properties()
    total_vesting_shares = gdgp.total_vesting_shares
    total_vesting_fund_hive = gdgp.total_vesting_fund_hive
    vesting_to_hive_feed = (total_vesting_shares.as_float() / 1000000) / (total_vesting_fund_hive.as_float() / 1000)
    return tt.Asset.Hive(vesting.as_float() / 1000000 / vesting_to_hive_feed)


class Comment:
    """Represents chain comment instance.


    +--------+
    |  top   |  <- created by self.reply
    +--------+
       /|\
        |
        |
    +--------+
    |  self  |
    +--------+
       /|\
        |
        |
    +--------+
    | bottom |  <- created by self.create_parent_comment
    +--------+

    bottom is post (it has no parent set)
    self is reply to bottom (it has parent_author and parent_permlink set to author and permlink of bottom)
    top is reply to self (it has parent_author and parent_permlink set to author and permlink of self)

    to achieve such schema you should:

    comment = Comment(node, wallet)
    comment.create_parent_comment()
    comment.post()
    comment.reply()
    """

    account_counter = 0

    def __init__(
        self,
        node: tt.InitNode,
        wallet: tt.Wallet,
        *,
        permlink: str | None = None,
        author: CommentAccount | None = None,
        child: Comment | None = None,
        parent: Comment | None = None,
    ):
        self.__node = node
        self.__wallet = wallet
        self.__author = author
        self.__parent = parent
        self.__permlink = permlink or f"main-permlink-{self.author}"
        self.__children: list[Comment] = [child] if child is not None else []
        self.__comment_transaction: CommentTransaction | None = None
        self.__comment_option_transaction: CommentOptionsTransaction
        self.__deleted: bool = False
        self.__beneficiaries: list[dict] = []
        self.__benefactors: list[str] = []

    @property
    def node(self) -> tt.InitNode:
        return self.__node

    @property
    def wallet(self) -> tt.Wallet:
        return self.__wallet

    @property
    def author_obj(self) -> CommentAccount:
        if self.__author is None:
            self.__author = self.__create_comment_account()
        return self.__author

    @property
    def author(self) -> str:
        return self.author_obj.name

    @property
    def permlink(self) -> str:
        return self.__permlink

    @property
    def comment_trx(self) -> CommentTransaction:
        assert self.comment_exists()
        return self.__comment_transaction

    @property
    def parent(self) -> Comment | None:
        return self.__parent

    @property
    def benefactors(self) -> list:
        return self.__benefactors

    def fill_beneficiares(self, beneficiaries: list, beneficiary_accounts: list) -> None:
        self.__beneficiaries = beneficiaries
        self.__benefactors = beneficiary_accounts

    def __force_get_parent(self) -> Comment:
        """
        if parent does not exists, returns non-existing comment with parameters as for new post
        """
        return self.parent or Comment(
            node=self.__node,
            wallet=self.__wallet,
            permlink="parent-permlink-is-not-empty",
            author=CommentAccount("", self.__node, self.__wallet),
        )

    @property
    def children(self) -> list[Comment]:
        return self.__children

    def comment_exists(self) -> bool:
        return self.__comment_transaction is not None and not self.__deleted

    def assert_comment_exists(self) -> None:
        if not self.comment_exists():
            raise ValueError("Comment not exist")

    def __create_comment_account(self) -> CommentAccount:
        author = f"account-{Comment.account_counter}"
        sample_vests_amount = 10000
        sample_hive_amount = 10000
        sample_hbds_amount = 10000

        self.__wallet.create_account(
            author, hives=sample_hive_amount, hbds=sample_hbds_amount, vests=sample_vests_amount
        )
        Comment.account_counter += 1
        return CommentAccount(author, self.__node, self.__wallet)

    def create_parent_comment(self) -> Comment:
        parent_account = self.__create_comment_account()
        self.__parent = Comment(
            node=self.__node,
            wallet=self.__wallet,
            permlink=f"parent-permlink-{parent_account.name}",
            author=parent_account,
            child=self,
        )
        self.__parent.send(reply_type="no_reply")
        return self.parent

    def send(
        self, reply_type: Literal["no_reply", "reply_own_comment", "reply_another_comment"], **comment_options: Any
    ) -> None:
        """
        Function for sending a comment.

        Parameters:
        reply_type (str): The type of reply to a comment. It can take one of the following values:
            - "no_reply": No reply.
            - "reply_own_comment": Reply to your own comment.
            - "reply_another_comment": Reply to another comment.

        **comment_options: Optional parameters related to the comment. It can include the following keys and their corresponding values:
            - max_accepted_payout (dict): Maximum accepted payout.
            - percent_hbd (int): The percentage of HBD rewards.
            - allow_votes (bool): Whether to allow votes.
            - allow_curation_rewards (bool): Whether to allow curation rewards.
            - beneficiaries (list): A list of beneficiaries for the comment, where each beneficiary is a dictionary.
        """

        def new_post() -> None:
            if self.__author is None:
                self.__author = self.__create_comment_account()

        def reply_to_parent_as_same_account():
            assert self.parent is not None
            self.__author = self.parent.author_obj
            self.__permlink = f"self-response-{self.parent.permlink}"

        def reply_to_parent_as_other_account():
            assert self.parent is not None
            if self.__author is None:
                self.__author = self.__create_comment_account()
            self.__permlink = f"{self.author}-response-{self.parent.permlink}"

        set_reply = {
            "no_reply": new_post,
            "reply_own_comment": reply_to_parent_as_same_account,
            "reply_another_comment": reply_to_parent_as_other_account,
        }

        set_reply[reply_type]()

        self.author_obj.update_account_info()  # Refresh RC mana before send
        with self.__wallet.in_single_transaction() as self.__comment_transaction:
            self.__wallet.api.post_comment(
                author=self.author,
                permlink=self.permlink,
                parent_author=self.__force_get_parent().author,
                parent_permlink=self.__force_get_parent().permlink if self.parent is None else self.parent.permlink,
                title=f"tittle-{self.permlink}",
                body=f"body-{self.permlink}",
                json="{}",
                only_result=False,
            )
            if comment_options != {}:
                self.__options(**comment_options)

        self.__comment_transaction = CommentTransaction(**self.__comment_transaction.get_response())

    def reply(self, reply_type: Literal["reply_own_comment", "reply_another_comment"]) -> Comment:
        self.assert_comment_exists()
        reply_account = self.author_obj
        reply_permlink = f"self-reply-{self.permlink}"

        if reply_type != "reply_own_comment":
            reply_account = self.__create_comment_account()
            reply_permlink = f"{reply_account.name}-reply-{self.permlink}"

        reply = Comment(
            node=self.__node,
            wallet=self.__wallet,
            permlink=reply_permlink,
            author=reply_account,
            parent=self,
        )
        reply.send(reply_type=reply_type)
        self.__children.append(reply)
        return reply

    def update(self) -> None:
        self.author_obj.update_account_info()  # Refresh RC mana before update
        self.__comment_transaction = get_response_model(
            CommentTransaction,
            **self.__wallet.api.post_comment(
                author=self.author,
                permlink=self.permlink,
                parent_author=self.__force_get_parent().author,
                parent_permlink=self.__force_get_parent().permlink,
                title=f"update-title-{self.author}",
                body=f"update-body-{self.permlink}",
                json='{"tags":["hiveio","example","tags"]}',
                only_result=False,
            ),
        ).result

    def vote(self) -> None:
        self.assert_comment_exists()
        voter = self.__create_comment_account()
        self.__wallet.api.vote(voter.name, self.author, self.permlink, 100)

    def downvote(self) -> None:
        self.assert_comment_exists()
        hater = self.__create_comment_account()
        self.__wallet.api.vote(hater.name, self.author, self.permlink, -10)

    def assert_is_comment_sent_or_update(self) -> None:
        comment_operation = self.comment_trx.operations[0]
        ops_in_block = self.__node.api.account_history.get_ops_in_block(
            block_num=self.comment_trx.block_num, include_reversible=True
        )
        for operation in ops_in_block.ops:
            if operation.op.type == "comment_operation" and operation.op.value == comment_operation.value:
                return
        raise AssertionError

    def assert_is_rc_mana_decreased_after_post_or_update(self) -> None:
        self.assert_comment_exists()
        comment_rc_cost = int(self.comment_trx.rc_cost)
        comment_timestamp = get_transaction_timestamp(self.__node, self.__comment_transaction)
        self.author_obj.rc_manabar.assert_rc_current_mana_is_reduced(comment_rc_cost, comment_timestamp)

    def delete(self) -> None:
        self.author_obj.update_account_info()  # Refresh RC mana before update
        self.__delete_transaction = create_transaction_with_any_operation(
            self.__wallet,
            DeleteCommentOperation(author=self.author, permlink=self.permlink),
        )

    def options(self, **comment_options: Any) -> None:
        """
        Function for modifying a comment options.

        **comment_options: Optional parameters related to the comment. It can include the following keys and their corresponding values:
            - max_accepted_payout (dict): Maximum accepted payout.
            - percent_hbd (int): The percentage of HBD rewards.
            - allow_votes (bool): Whether to allow votes.
            - allow_curation_rewards (bool): Whether to allow curation rewards.
            - beneficiaries (list): A list of beneficiaries for the comment, where each beneficiary is a dictionary.
        """

        self.__author.update_account_info()  # Refresh RC mana before comment_options_operation
        self.__comment_option_transaction = self.__options(**comment_options)

    def assert_rc_mana_after_change_comment_options(self, mode: Literal["decrease", "is_unchanged"]) -> None:
        if mode == "decrease":
            comment_option_rc_cost = int(self.__comment_option_transaction.rc_cost)
            comment_option_timestamp = get_transaction_timestamp(self.__node, self.__comment_option_transaction)
            self.__author.rc_manabar.assert_rc_current_mana_is_reduced(comment_option_rc_cost, comment_option_timestamp)
        elif mode == "is_unchanged":
            self.__author.rc_manabar.assert_current_mana_is_unchanged()
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def __options(self, **comment_options: Any) -> CommentOptionsTransaction:
        """
        Private function for modifying a comment options.

        **comment_options: Optional parameters related to the comment. It can include the following keys and their corresponding values:
            - max_accepted_payout (dict): Maximum accepted payout.
            - percent_hbd (int): The percentage of HBD rewards.
            - allow_votes (bool): Whether to allow votes.
            - allow_curation_rewards (bool): Whether to allow curation rewards.
            - beneficiaries (list): A list of beneficiaries for the comment, where each beneficiary is a dictionary.
        """
        self.__comment_options = comment_options

        if "beneficiaries" in comment_options:
            comment_options["extensions"] = [
                [
                    "comment_payout_beneficiaries",
                    {"beneficiaries": comment_options["beneficiaries"]},
                ]
            ]
            comment_options.pop("beneficiaries")

        comment_options_operation = CommentOptionsOperationLegacy(
            author=self.__author.name,
            permlink=self.__permlink,
            **comment_options,
        )

        comment_options_operation.max_accepted_payout = self.__convert_from_mainnet_to_testnet_asset(
            comment_options_operation.max_accepted_payout
        )

        return get_response_model(
            CommentOptionsTransaction,
            **create_transaction_with_any_operation(
                self.__wallet,
                comment_options_operation,
                only_result=False,
            ),
        ).result

    def assert_options_are_apply(self) -> None:
        comment_content = self.__node.api.database.find_comments(comments=[[self.author, self.__permlink]]).comments[0]

        for key in ["max_accepted_payout", "percent_hbd", "allow_votes", "allow_curation_rewards", "beneficiaries"]:
            if key in self.__comment_options:
                if key != "max_accepted_payout":
                    assert self.__comment_options[key] == getattr(comment_content, key), f"{key} is not applied"
                else:
                    assert (
                        tt.Asset.from_legacy(self.__comment_options[key]).amount == getattr(comment_content, key).amount
                    )

    def __convert_from_mainnet_to_testnet_asset(self, asset: tt.Asset.AnyT) -> tt.Asset.AnyT:
        asset = asset.as_nai()
        return tt.Asset.from_nai(asset).as_legacy()

    def assert_is_rc_mana_decreased_after_comment_delete(self) -> None:
        self.assert_comment_exists()
        delete_rc_cost = int(self.__delete_transaction["rc_cost"])
        delete_timestamp = get_transaction_timestamp(self.__node, self.__delete_transaction)
        self.author_obj.rc_manabar.assert_rc_current_mana_is_reduced(delete_rc_cost, delete_timestamp)

    def assert_comment(self, mode: Literal["deleted", "not_deleted"]) -> None:
        voter = self.__create_comment_account()
        try:
            self.__wallet.api.vote(voter.name, self.author, self.permlink, 1)
            vote_send = True
        except tt.exceptions.CommunicationError:
            vote_send = False

        if mode == "deleted":
            assert vote_send is False, "Vote send. Comment is not deleted"
        elif mode == "not_deleted":
            assert vote_send is True, "Vote not send. Comment is deleted"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def get_author_reputation(self) -> HiveInt:
        reputations = self.__node.api.reputation.get_account_reputations(account_lower_bound="", limit=1000).reputations
        for account_reputation in reputations:
            if account_reputation.account == self.__author.name:
                return account_reputation.reputation
        raise ValueError(f"Cannot get reputation for account: {self.__author.name}")

    def assert_author_reward_virtual_operation(self, mode: Literal["generated", "not_generated"]) -> None:
        author_reward_operations = get_reward_operations(self.__node, "author")
        author_and_permlink = [(value.author, value.permlink) for value in author_reward_operations]
        if mode == "generated":
            assert (
                self.author,
                self.permlink,
            ) in author_and_permlink, "Author_reward_operation not generated, but it should have been"
        elif mode == "not_generated":
            assert (
                self.author,
                self.permlink,
            ) not in author_and_permlink, "Author_reward_operation generated, but it should have not been"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def assert_comment_benefactors_reward_virtual_operations(self, mode: Literal["generated", "not_generated"]) -> None:
        comment_benefactors_operations = get_reward_operations(self.__node, "comment_benefactor")
        for beneficiary in self.__beneficiaries:
            benefactor_and_permlink = [(value.benefactor, value.permlink) for value in comment_benefactors_operations]
            if mode == "generated":
                assert (
                    beneficiary["account"],
                    self.permlink,
                ) in benefactor_and_permlink, (
                    "Comment_benefactor_reward_operation not generated, but it should have been"
                )
            elif mode == "not_generated":
                assert (
                    beneficiary["account"],
                    self.permlink,
                ) not in benefactor_and_permlink, (
                    "Comment_benefactor_reward_operation generated, but it should have not been"
                )
            else:
                raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def get_reward(
        self,
        reward_type: Literal["author", "comment_benefactor"],
        mode: Literal["hbd_payout", "vesting_payout"],
        benefactor: str | None = None,
    ):
        assert mode in ["hbd_payout", "vesting_payout"], f"Unexpected value for 'mode': '{mode}'"
        if reward_type == "comment_benefactor":
            assert benefactor is not None, "You have to provide benefactor."

        reward_operations = get_reward_operations(self.__node, reward_type)
        for value in reward_operations:
            condition = (value.author == self.author) if reward_type == "author" else (value.benefactor == benefactor)
            if condition and value.permlink == self.permlink:
                return getattr(value, mode)
        raise ValueError(f"Comment not have generated {reward_type}_reward_operation")

    def assert_resource_percentage_in_reward(
        self,
        reward_type: Literal["author", "comment_benefactor"],
        hbd_percentage: int,
        vesting_percentage: int,
        benefactor: str | None = None,
    ):
        if reward_type == "author":
            assert benefactor is None, "You have not to provide benefactor."

        if benefactor is None and reward_type == "comment_benefactor":
            benefactor = self.__benefactors[0].name

        hbd_reward = self.get_reward(reward_type=reward_type, mode="hbd_payout", benefactor=benefactor)
        hbd_reward_as_hive = convert_hbd_to_hive(self.__node, hbd_reward)
        vesting_payout = self.get_reward(reward_type=reward_type, mode="vesting_payout", benefactor=benefactor)
        vesting_payout_as_hive = convert_vesting_to_hive(self.__node, vesting_payout)
        assert (
            abs(hbd_reward_as_hive.as_float() * vesting_percentage - vesting_payout_as_hive.as_float() * hbd_percentage)
            <= 100
        ), "Resources percentage in reward are incorrect"

    def verify_balances(self):
        self.__author.assert_reward_balance(self.node, "hbd")
        self.__author.assert_reward_balance(self.node, "vesting")
        if self.__benefactors:
            for benefactor in self.__benefactors:
                benefactor.assert_reward_balance(self.node, "hbd")
                benefactor.assert_reward_balance(self.node, "vesting")


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


class Vote:
    account_counter = 0

    def __init__(self, comment_obj: Comment, voter: Literal["random", "same_as_comment"] | CommentAccount):
        self.__comment_obj = comment_obj
        self.__voter = voter
        self.__vote_transaction: TransactionLegacy | None = None
        if isinstance(voter, Account):
            self.__voter_obj = voter
        else:
            match voter:
                case "same_as_comment":
                    self.__voter_obj = comment_obj.author_obj
                case "random":
                    self.__voter_obj = self.__create_voter_account()
                case _:
                    raise ValueError

    @property
    def voter(self) -> str:
        return self.__voter_obj.name

    def __create_voter_account(self) -> CommentAccount:
        author = f"voter-{Vote.account_counter}"
        sample_vests_amount = 50
        self.__comment_obj.wallet.create_account(author, vests=sample_vests_amount)
        Vote.account_counter += 1
        return CommentAccount(author, self.__comment_obj.node, self.__comment_obj.wallet)

    def assert_vote(self, mode: Literal["occurred", "not_occurred"]) -> None:
        if mode == "occurred":
            vote_operation = self.__vote_transaction["operations"][0][1]
            operation_values = []
            for i in (1, 2):
                operations = self.__comment_obj.node.api.account_history.get_ops_in_block(
                    block_num=self.__vote_transaction["ref_block_num"] + i, include_reversible=True
                ).ops
                operation_values = operation_values + [operation.op.value for operation in operations]
            assert vote_operation in operation_values, "Vote_operation not generated, but it should have been"
        elif mode == "not_occurred":
            assert self.__vote_transaction is None, "Vote_operation generated, but it should have not been"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def assert_rc_mana_after_vote_or_downvote(self, mode: Literal["decrease", "is_unchanged"]) -> None:
        if mode == "decrease":
            vote_rc_cost = int(self.__vote_transaction["rc_cost"])
            vote_timestamp = get_transaction_timestamp(self.__comment_obj.node, self.__vote_transaction)
            self.__voter_obj.rc_manabar.assert_rc_current_mana_is_reduced(vote_rc_cost, vote_timestamp)
        elif mode == "is_unchanged":
            self.__voter_obj.rc_manabar.assert_current_mana_is_unchanged()
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def assert_vote_or_downvote_manabar(
        self, manabar_type: Literal["vote_manabar", "downvote_manabar"], mode: Literal["decrease", "is_unchanged"]
    ) -> None:
        assert manabar_type in ("vote_manabar", "downvote_manabar"), "Wrong manabar type"
        if mode == "decrease":
            vote_timestamp = get_transaction_timestamp(self.__comment_obj.node, self.__vote_transaction)
            getattr(self.__voter_obj, manabar_type).assert_current_mana_is_reduced(vote_timestamp)
        elif mode == "is_unchanged":
            getattr(self.__voter_obj, manabar_type).assert_current_mana_is_unchanged()
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def assert_effective_comment_vote_operation(self, mode: Literal["generated", "not_generated"]) -> None:
        vops = get_virtual_operations(self.__comment_obj.node, EffectiveCommentVoteOperation)
        effective_comment_vote_operations = [vop["op"]["value"] for vop in vops]
        voter_and_comment_permlink = [(vop.voter, vop.permlink) for vop in effective_comment_vote_operations]
        if mode == "generated":
            assert (
                self.__voter_obj.name,
                self.__comment_obj.permlink,
            ) in voter_and_comment_permlink, "Effective_comment_vote_operation not generated, but it should have been"
        elif mode == "not_generated":
            assert (
                self.__voter_obj.name,
                self.__comment_obj.permlink,
            ) not in voter_and_comment_permlink, (
                "Effective_comment_vote_operation generated, but it should have not been"
            )
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def __update_account_info_and_execute_vote(self, weight: int) -> None:
        self.__voter_obj.update_account_info()  # Refresh RC mana and Vote mana before vote
        self.__vote_transaction = self.__comment_obj.wallet.api.vote(
            self.__voter_obj.name, self.__comment_obj.author, self.__comment_obj.permlink, weight
        )

    def vote(self, weight: int) -> None:
        if not 0 <= weight <= 100:
            raise ValueError(f"Vote with weight {weight} is not allowed. Weight can take (0-100)")
        self.__update_account_info_and_execute_vote(weight)

    def downvote(self, weight: int) -> None:
        if not -100 <= weight <= 0:
            raise ValueError(f"Downvote with weight {weight} is not allowed. Weight can take (-100-0)")
        self.__update_account_info_and_execute_vote(weight)

    def assert_curation_reward_virtual_operation(self, mode: Literal["generated", "not_generated"]) -> None:
        curation_reward_operations = get_reward_operations(self.__comment_obj.node, "curation")
        curator_and_comment_permlink = [(value.curator, value.permlink) for value in curation_reward_operations]
        if mode == "generated":
            assert (
                self.__voter.name,
                self.__comment_obj.permlink,
            ) in curator_and_comment_permlink, "Curation_reward_operation not generated, but it should have been"
        elif mode == "not_generated":
            assert (
                self.__voter.name,
                self.__comment_obj.permlink,
            ) not in curator_and_comment_permlink, "Curation_reward_operation generated, but it should have not been"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def get_curation_vesting_reward(self) -> tt.Asset.VestsT:
        """Curation reward is always paid in HP"""
        curation_reward_operations = get_reward_operations(self.__comment_obj.node, "curation")
        for value in curation_reward_operations:
            if value.curator == self.__voter.name and value.permlink == self.__comment_obj.permlink:
                return value.reward
        raise ValueError("Vote not have generated curation_reward_operation")


class CommentAccount(Account):
    def __init__(self, name: str, node: tt.AnyNode, wallet: tt.Wallet) -> None:
        super().__init__(_name=name, _node=node, _wallet=wallet)

    def __get_rewards_from_reward_operation(
        self,
        node: tt.AnyNode,
        mode: Literal["author", "curation", "comment_benefactor"],
        reward_type: Literal["vesting_payout", "hbd_payout"],
    ) -> list:
        reward_operations = get_reward_operations(node, mode)
        if mode == "author" and reward_type is not None:
            return [getattr(vop, reward_type) for vop in reward_operations if vop.author == self.name]
        if mode == "comment_benefactor" and reward_type is not None:
            return [getattr(vop, reward_type) for vop in reward_operations if vop.benefactor == self.name]
        if mode == "curation" and reward_type == "vesting_payout":
            return [vop.reward for vop in reward_operations if vop.curator == self.name]
        raise ValueError(f"Wrong argument combination: 'mode': '{mode} and 'reward_type': {reward_type}")

    def assert_reward_balance(self, node: tt.Anynode, mode: Literal["hbd", "vesting"]) -> None:
        if mode not in ["hbd", "vesting"]:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

        account_reward_balance = (
            get_reward_hbd_balance(node, self.name) if mode == "hbd" else get_reward_vesting_balance(node, self.name)
        )

        account_rewards_from_author_reward_operation = self.__get_rewards_from_reward_operation(
            node, "author", f"{mode}_payout"
        )

        account_rewards_from_comment_benefactor_reward_operation = self.__get_rewards_from_reward_operation(
            node, "comment_benefactor", f"{mode}_payout"
        )

        account_rewards_from_curation_reward_operation = (
            self.__get_rewards_from_reward_operation(node, "curation", "vesting_payout")
            if mode == "vesting"
            else [tt.Asset.Tbd(0)]
        )

        sum_of_all_rewards = (
            sum(account_rewards_from_author_reward_operation)
            + sum(account_rewards_from_comment_benefactor_reward_operation)
            + sum(account_rewards_from_curation_reward_operation)
        )

        sum_of_all_rewards = (
            (tt.Asset.Tbd(sum_of_all_rewards) if mode == "hbd" else tt.Asset.Vest(sum_of_all_rewards))
            if sum_of_all_rewards == 0
            else sum_of_all_rewards
        )

        assert account_reward_balance == sum_of_all_rewards, f"{mode} reward has incorrect amount"

    def assert_curation_reward_virtual_operation(self, mode: Literal["generated", "not_generated"]) -> None:
        curation_reward_operations = get_reward_operations(self.node, "curation")
        curator = [value["curator"] for value in curation_reward_operations]
        if mode == "generated":
            assert self.name in curator, "Curation_reward_operation not generated, but it should have been"
        elif mode == "not_generated":
            assert self.name not in curator, "Curation_reward_operation generated, but it should have not been"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")
