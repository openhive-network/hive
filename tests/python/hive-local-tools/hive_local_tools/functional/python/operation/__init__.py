from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from typing import TYPE_CHECKING, Any, Literal

import test_tools as tt
import wax
from hive_local_tools.constants import (
    get_transaction_model,
)
from schemas.apis.database_api.fundaments_of_reponses import AccountItemFundament
from schemas.fields.compound import Manabar
from schemas.jsonrpc import get_response_model
from schemas.operations import (
    CommentOperation,
    DeleteCommentOperation,
)
from schemas.operations.representations.legacy_representation import LegacyRepresentation  # noqa: TCH001
from schemas.operations.virtual.fill_transfer_from_savings_operation import FillTransferFromSavingsOperation
from schemas.operations.virtual.transfer_to_vesting_completed_operation import (
    TransferToVestingCompletedOperation,
)
from schemas.transaction import TransactionLegacy
from schemas.virtual_operation import (
    VirtualOperation as SchemaVirtualOperation,
)
from schemas.virtual_operation import (
    build_vop_filter,
)

if TYPE_CHECKING:
    from schemas.apis.account_history_api.response_schemas import EnumVirtualOps
    from schemas.operations import AnyLegacyOperation


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
    def vest(self) -> tt.Asset.VestsT:
        return self._acc_info.vesting_shares

    def fund_vests(self, tests: tt.Asset.Test) -> None:
        self._wallet.api.transfer_to_vesting(
            "initminer",
            self._name,
            tests,
        )
        self.update_account_info()

    def get_hbd_balance(self) -> tt.Asset.TbdT:
        return get_hbd_balance(self._node, self._name)

    def get_hive_balance(self) -> tt.Asset.TestT:
        return get_hive_balance(self._node, self._name)

    def get_hive_power(self) -> tt.Asset.VestsT:
        return get_hive_power(self._node, self._name)

    def get_rc_current_mana(self) -> int:
        return get_rc_current_mana(self._node, self._name)

    def get_rc_max_mana(self) -> int:
        return get_rc_max_mana(self._node, self._name)

    def update_account_info(self) -> None:
        self._acc_info = _find_account(self._node, self._name)
        self._rc_manabar.update()

    def top_up(self, amount: tt.Asset.TestT | tt.Asset.TbdT) -> None:
        self._wallet.api.transfer("initminer", self._name, amount, "{}")
        self.update_account_info()


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
    def _get_specific_manabar(self):
        pass

    @abstractmethod
    def _get_specific_current_mana(self):
        pass

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

    def _get_specific_manabar(self):
        return get_rc_manabar(self._node, self._name)

    def _get_specific_current_mana(self):
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


def check_if_fill_transfer_from_savings_vop_was_generated(node: tt.InitNode, memo: str) -> bool:
    payout_vops = get_virtual_operations(node, FillTransferFromSavingsOperation)
    return any(vop["op"]["value"]["memo"] == memo for vop in payout_vops)


def create_transaction_with_any_operation(wallet: tt.Wallet, *operations: AnyLegacyOperation) -> dict[str, Any]:
    # function creates transaction manually because some operations are not added to wallet
    transaction = get_transaction_model()
    transaction.operations = [(op.get_name(), op) for op in operations]
    return wallet.api.sign_transaction(transaction)


def get_governance_voting_power(node: tt.InitNode, wallet: tt.Wallet, account_name: str) -> int:
    try:
        wallet.api.set_voting_proxy(account_name, "initminer")
    except tt.exceptions.CommunicationError as error:
        if "Proxy must change" not in str(error):
            raise

    return int(_find_account(node, "initminer").proxied_vsf_votes[0])


def get_hbd_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TbdT:
    return _find_account(node, account_name).hbd_balance


def get_hbd_savings_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TbdT:
    return _find_account(node, account_name).savings_hbd_balance


def get_hive_balance(node: tt.InitNode, account_name: str) -> tt.Asset.TestT:
    return _find_account(node, account_name).balance


def get_hive_power(node: tt.InitNode, account_name: str) -> tt.Asset.VestsT:
    return _find_account(node, account_name).vesting_shares


def get_rc_current_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name]).rc_accounts[0].rc_manabar.current_mana)


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
) -> list:
    """
    :param vop: name of the virtual operation,
    :param skip_price_stabilization: by default, removes from the list operations confirming vesting-stabilizing,
    :param start_block: block from which virtual operations will be given,
    :return: a list of virtual operations of the type specified in the `vop` argument.
    """
    result: EnumVirtualOps = node.api.account_history.enum_virtual_ops(
        filter=build_vop_filter(*vops),
        include_reversible=True,
        block_range_begin=start_block,
        block_range_end=2000,
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


def get_transaction_timestamp(node: tt.InitNode, transaction):
    return tt.Time.parse(node.api.block.get_block(block_num=transaction["block_num"])["block"]["timestamp"])


def jump_to_date(node: tt.InitNode, time_offset: datetime) -> None:
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
    operations: list[LegacyRepresentation[CommentOperation]]
    rc_cost: int


def list_votes_for_all_proposals(node):
    return node.api.database.list_proposal_votes(
        start=[""], limit=1000, order="by_voter_proposal", order_direction="ascending", status="all"
    )["proposal_votes"]


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
        author: Account | None = None,
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
        self.__deleted: bool = False

    @property
    def author_obj(self) -> Account:
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

    def __force_get_parent(self) -> Comment:
        """
        if parent does not exists, returns non-existing comment with parameters as for new post
        """
        return self.parent or Comment(
            node=self.__node,
            wallet=self.__wallet,
            permlink="parent-permlink-is-not-empty",
            author=Account("", self.__node, self.__wallet),
        )

    @property
    def children(self) -> list[Comment]:
        return self.__children

    def comment_exists(self) -> bool:
        return self.__comment_transaction is not None and not self.__deleted

    def assert_comment_exists(self) -> None:
        if not self.comment_exists():
            raise ValueError("Comment not exist")

    def __create_comment_account(self) -> Account:
        author = f"account-{Comment.account_counter}"
        sample_vests_amount = 50
        self.__wallet.create_account(author, vests=sample_vests_amount)
        Comment.account_counter += 1
        return Account(author, self.__node, self.__wallet)

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
        self,
        reply_type: Literal["no_reply", "reply_own_comment", "reply_another_comment"],
    ) -> None:
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
        self.__comment_transaction = get_response_model(
            CommentTransaction,
            **self.__wallet.api.post_comment(
                author=self.author,
                permlink=self.permlink,
                parent_author=self.__force_get_parent().author,
                parent_permlink=self.__force_get_parent().permlink if self.parent is None else self.parent.permlink,
                title=f"tittle-{self.permlink}",
                body=f"body-{self.permlink}",
                json="{}",
                only_result=False,
            ),
        ).result

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
