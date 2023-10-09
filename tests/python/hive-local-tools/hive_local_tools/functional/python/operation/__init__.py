from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Literal, Union

import test_tools as tt
import wax
from hive_local_tools.constants import TRANSACTION_TEMPLATE, filters_enum_virtual_ops


@dataclass
class Operation:
    _node: tt.InitNode
    _wallet: tt.Wallet
    _rc_cost: int = field(init=False, default=None)

    def assert_minimal_operation_rc_cost(self):
        assert self._rc_cost > 0, "RC cost is less than or equal to zero."


@dataclass
class Account:
    _acc_info: dict = field(init=False, default_factory=dict)
    _name: str
    _node: tt.InitNode
    _wallet: tt.Wallet
    _hive: tt.Asset.Test = field(init=False, default=None)
    _vest: tt.Asset.Vest = field(init=False, default=None)
    _hbd: tt.Asset.Tbd = field(init=False, default=None)

    def __post_init__(self):
        self._rc_manabar = _RcManabar(self._node, self._name)

    @property
    def node(self):
        return self._node

    @property
    def name(self):
        return self._name

    @property
    def hive(self):
        return self._hive

    @property
    def hbd(self):
        return self._hbd

    @property
    def rc_manabar(self):
        return self._rc_manabar

    @property
    def vest(self):
        return self._vest

    def get_hbd_balance(self):
        return get_hbd_balance(self._node, self._name)

    def get_hive_balance(self):
        return get_hive_balance(self._node, self._name)

    def get_hive_power(self):
        return get_hive_power(self._node, self._name)

    def get_rc_current_mana(self):
        return get_rc_current_mana(self._node, self._name)

    def get_rc_max_mana(self):
        return get_rc_max_mana(self._node, self._name)

    def update_account_info(self):
        self._acc_info = self._node.api.database.find_accounts(accounts=[self._name])["accounts"][0]
        self._hive = tt.Asset.from_(self._acc_info["balance"])
        self._hbd = tt.Asset.from_(self._acc_info["hbd_balance"])
        self._vest = tt.Asset.from_(self._acc_info["vesting_shares"])
        self._rc_manabar.update()

    def top_up(self, amount: Union[tt.Asset.Test, tt.Asset.Tbd]):
        self._wallet.api.transfer("initminer", self._name, amount, "{}")
        self.update_account_info()


class _RcManabar:
    def __init__(self, node, name):
        self._node = node
        self._name = name
        self.current_rc_mana = None
        self.last_update_time = None
        self.max_rc = None
        self.update()

    def __str__(self):
        return (
            f"max_rc: {self.max_rc}, current_mana: {self.current_rc_mana}, last_update_time:"
            f" {datetime.fromtimestamp(self.last_update_time)}"
        )

    def calculate_current_value(self, head_block_time):
        return int(
            wax.calculate_current_manabar_value(
                now=int(head_block_time.timestamp()),
                max_mana=self.max_rc,
                current_mana=self.current_rc_mana,
                last_update_time=self.last_update_time,
            ).result
        )

    def update(self):
        rc_manabar = get_rc_manabar(self._node, self._name)
        self.current_rc_mana = rc_manabar["current_mana"]
        self.last_update_time = rc_manabar["last_update_time"]
        self.max_rc = rc_manabar["max_rc"]

    def assert_rc_current_mana_is_unchanged(self):
        assert (
            get_rc_current_mana(self._node, self._name) == self.current_rc_mana
        ), f"The {self._name} account rc_current_mana has been changed."

    def assert_rc_current_mana_is_reduced(self, operation_rc_cost: int, operation_timestamp=None):
        err = f"The account {self._name} did not incur the operation cost."
        if operation_timestamp:
            mana_before_operation = self.calculate_current_value(operation_timestamp - tt.Time.seconds(3))
            assert mana_before_operation == get_rc_current_mana(self._node, self._name) + operation_rc_cost, err
        else:
            assert get_rc_current_mana(self._node, self._name) + operation_rc_cost == self.current_rc_mana, err

    def assert_max_rc_mana_state(self, state: Literal["reduced", "unchanged", "increased"]):
        actual_max_rc_mana = get_rc_max_mana(self._node, self._name)
        error_message = f"The {self._name} account `rc_max_mana` has been not {state}"
        match state:
            case "unchanged":
                assert actual_max_rc_mana == self.max_rc, error_message
            case "reduced":
                assert actual_max_rc_mana < self.max_rc, error_message
            case "increased":
                assert actual_max_rc_mana > self.max_rc, error_message


def check_if_fill_transfer_from_savings_vop_was_generated(node: tt.InitNode, memo: str) -> bool:
    payout_vops = get_virtual_operations(node, "fill_transfer_from_savings_operation")
    for vop in payout_vops:
        if vop["op"]["value"]["memo"] == memo:
            return True
    return False


def create_transaction_with_any_operation(wallet, operation_name, **kwargs):
    # function creates transaction manually because some operations are not added to wallet
    transaction = deepcopy(TRANSACTION_TEMPLATE)
    transaction["operations"].append([operation_name, kwargs])
    return wallet.api.sign_transaction(transaction)


def get_governance_voting_power(node: tt.InitNode, wallet: tt.Wallet, account_name: str) -> int:
    try:
        wallet.api.set_voting_proxy(account_name, "initminer")
    finally:
        return int(node.api.database.find_accounts(accounts=["initminer"])["accounts"][0]["proxied_vsf_votes"][0])


def get_hbd_balance(node: tt.InitNode, account_name: str) -> tt.Asset.Tbd:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["hbd_balance"])


def get_hbd_savings_balance(node: tt.InitNode, account_name: str) -> tt.Asset.Tbd:
    return tt.Asset.from_(
        node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["savings_hbd_balance"]
    )


def get_hive_balance(node: tt.InitNode, account_name: str) -> tt.Asset.Test:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["balance"])


def get_hive_power(node: tt.InitNode, account_name: str) -> tt.Asset.Vest:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["vesting_shares"])


def get_rc_current_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name])["rc_accounts"][0]["rc_manabar"]["current_mana"])


def get_number_of_fill_order_operations(node):
    return len(
        node.api.account_history.enum_virtual_ops(
            block_range_begin=1,
            block_range_end=node.get_last_block_number() + 1,
            include_reversible=True,
            filter=0x000080,
            group_by_block=False,
        )["ops"]
    )


def get_vesting_price(node: tt.InitNode) -> int:
    """
    Current exchange rate - `1` Hive to Vest conversion price.
    """
    dgpo = node.api.wallet_bridge.get_dynamic_global_properties()
    total_vesting_shares = dgpo["total_vesting_shares"]["amount"]
    total_vesting_fund_hive = dgpo["total_vesting_fund_hive"]["amount"]

    return int(total_vesting_shares) // int(total_vesting_fund_hive)


def convert_hive_to_vest_range(hive_amount: tt.Asset.Test, price: float, tolerance: int = 5) -> tt.Asset.Range:
    """
     Converts Hive to VEST resources at the current exchange rate provided in the `price` parameter.
    :param hive_amount: The amount of Hive resources to convert.
    :param price: The current exchange rate for the conversion.
    :param tolerance: The tolerance percent for the VEST conversion, defaults its 5%.
    :return: The equivalent amount of VEST resources after the conversion, within the specified tolerance.
    """
    vests = tt.Asset.from_({"amount": hive_amount.amount * price, "precision": 6, "nai": "@@000000037"})
    return tt.Asset.Range(vests, tolerance=tolerance)


def get_virtual_operations(
    node: tt.InitNode, vop: str, skip_price_stabilization: bool = True, start_block: int = None
) -> list:
    """
    :param vop: name of the virtual operation,
    :param skip_price_stabilization: by default, removes from the list operations confirming vesting-stabilizing,
    :param start_block: block from which virtual operations will be given,
    :return: a list of virtual operations of the type specified in the `vop` argument.
    """
    result = node.api.account_history.enum_virtual_ops(
        filter=filters_enum_virtual_ops[vop],
        include_reversible=True,
        block_range_begin=start_block,
        block_range_end=2000,
    )["ops"]

    if skip_price_stabilization and vop == "transfer_to_vesting_completed_operation":
        for vop_number, vop in enumerate(result):
            if vop["op"]["value"]["hive_vested"] == tt.Asset.Test(10_000_000):
                result.pop(vop_number)
    return result


def get_rc_max_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name])["rc_accounts"][0]["max_rc"])


def get_transaction_timestamp(node: tt.InitNode, transaction):
    return tt.Time.parse(node.api.block.get_block(block_num=transaction["block_num"])["block"]["timestamp"])


def jump_to_date(node: tt.InitNode, time_offset: datetime) -> None:
    node.restart(
        time_offset=tt.Time.serialize(
            time_offset,
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )


def get_rc_manabar(node: tt.InitNode, account_name: str) -> dict:
    response = node.api.rc.find_rc_accounts(accounts=[account_name])["rc_accounts"][0]
    return {
        "current_mana": int(response["rc_manabar"]["current_mana"]),
        "last_update_time": response["rc_manabar"]["last_update_time"],
        "max_rc": int(response["max_rc"]),
    }


class Comment:
    account_counter = 0

    def __init__(self, node: tt.InitNode, wallet: tt.Wallet):
        self.__node = node
        self.__wallet = wallet
        self.__comment_transaction: dict[str, Any] | None = None
        self.__bottom_comment_sent: bool = False
        self.__comment_exist: bool = False
        self.__comment_deleted: bool = False

    @property
    def author(self):
        return self.__comment_author_obj.name

    def __create_comment_account(self):
        author = f"account-{Comment.account_counter}"
        sample_vests_amount = 50
        self.__wallet.create_account(author, vests=sample_vests_amount)
        Comment.account_counter += 1
        return Account(author, self.__node, self.__wallet)

    def send_bottom_comment(self):
        self.__bottom_comment_author_obj = self.__create_comment_account()
        self.__bottom_comment_permlink = f"bottom-permlink-{self.__bottom_comment_author_obj.name}"
        self.__wallet.api.post_comment(
            author=self.__bottom_comment_author_obj.name,
            permlink=self.__bottom_comment_permlink,
            parent_author="",
            parent_permlink="parent-permlink-is-not-empty",
            title=f"parent-title-{self.__bottom_comment_author_obj.name}",
            body=f"parent-body-{self.__bottom_comment_author_obj.name}",
            json="{}",
        )
        self.__bottom_comment_sent = True

    def send(self, reply_type: Literal["no_reply", "reply_own_comment", "reply_another_comment"]):
        if not self.__bottom_comment_sent and reply_type in {"reply_own_comment", "reply_another_comment"}:
            raise ValueError("Parent comment not exist")

        def set_no_reply():
            self.__parent_author: str = ""
            self.__parent_permlink: str = "parent-permlink-is-not-empty"
            self.__comment_author_obj = self.__create_comment_account()

        def set_reply_own_comment():
            self.__parent_author: str = self.__bottom_comment_author_obj.name
            self.__parent_permlink: str = self.__bottom_comment_permlink
            self.__comment_author_obj = self.__bottom_comment_author_obj

        def set_reply_another_comment():
            self.__parent_author: str = self.__bottom_comment_author_obj.name
            self.__parent_permlink: str = self.__bottom_comment_permlink
            self.__comment_author_obj = self.__create_comment_account()

        set_reply = {
            "no_reply": set_no_reply,
            "reply_own_comment": set_reply_own_comment,
            "reply_another_comment": set_reply_another_comment,
        }

        set_reply[reply_type]()
        self.__comment_permlink = f"main-permlink-{self.__comment_author_obj.name}"

        self.__comment_author_obj.update_account_info()  # Refresh RC mana before send
        self.__comment_transaction = self.__wallet.api.post_comment(
            author=self.__comment_author_obj.name,
            permlink=self.__comment_permlink,
            parent_author=self.__parent_author,
            parent_permlink=self.__parent_permlink,
            title=f"main-title-{self.__comment_author_obj.name}",
            body=f"main-body-{self.__comment_author_obj.name}",
            json="{}",
        )
        self.__comment_exist = True

    def send_top_comment(self, reply_type: Literal["reply_own_comment", "reply_another_comment"]):
        if not self.__comment_exist:
            raise ValueError("Parent comment not exist")

        parent_author: str = self.__comment_author_obj.name
        parent_permlink: str = self.__comment_permlink
        if reply_type == "reply_own_comment":
            top_comment_author_obj = self.__comment_author_obj
        else:
            top_comment_author_obj = self.__create_comment_account()

        self.__wallet.api.post_comment(
            author=top_comment_author_obj.name,
            permlink=f"top-permlink-{top_comment_author_obj.name}",
            parent_author=parent_author,
            parent_permlink=parent_permlink,
            title=f"top-title-{top_comment_author_obj.name}",
            body=f"top-body-{top_comment_author_obj.name}",
            json="{}",
        )

    def update(self):
        self.__comment_author_obj.update_account_info()  # Refresh RC mana before update
        self.__comment_transaction = self.__wallet.api.post_comment(
            author=self.__comment_author_obj.name,
            permlink=self.__comment_permlink,
            parent_author=self.__parent_author,
            parent_permlink=self.__parent_permlink,
            title=f"update-title-{self.__comment_author_obj.name}",
            body=f"update--body-{self.__comment_author_obj.name}",
            json='{"tags":["hiveio","example","tags"]}',
        )

    def vote(self):
        if not self.__comment_exist:
            raise ValueError("Comment not exist")
        voter = self.__create_comment_account()
        self.__wallet.api.vote(voter.name, self.__comment_author_obj.name, self.__comment_permlink, 100)

    def downvote(self):
        if not self.__comment_exist:
            raise ValueError("Comment not exist")
        hater = self.__create_comment_account()
        self.__wallet.api.vote(hater.name, self.__comment_author_obj.name, self.__comment_permlink, -10)

    def assert_is_comment_sent_or_update(self):
        comment_operation = self.__comment_transaction["operations"][0][1]
        ops_in_block = self.__node.api.account_history.get_ops_in_block(
            block_num=self.__comment_transaction["block_num"], include_reversible=True
        )
        for operation in ops_in_block["ops"]:
            if operation["op"]["type"] == "comment_operation" and operation["op"]["value"] == comment_operation:
                return
        assert False

    def assert_is_rc_mana_decreased_after_post_or_update(self):
        if not self.__comment_exist:
            raise ValueError("Try to post comment before verification")
        comment_rc_cost = int(self.__comment_transaction["rc_cost"])
        comment_timestamp = get_transaction_timestamp(self.__node, self.__comment_transaction)
        self.__comment_author_obj.rc_manabar.assert_rc_current_mana_is_reduced(comment_rc_cost, comment_timestamp)

    def delete(self):
        self.__comment_author_obj.update_account_info()  # Refresh RC mana before update
        self.__delete_transaction = create_transaction_with_any_operation(
            self.__wallet, "delete_comment", author=self.__comment_author_obj.name, permlink=self.__comment_permlink
        )
        self.__comment_deleted = True

    def assert_is_rc_mana_decreased_after_comment_delete(self):
        if not self.__comment_deleted:
            raise ValueError("Try to delete comment before verification")
        delete_rc_cost = int(self.__delete_transaction["rc_cost"])
        delete_timestamp = get_transaction_timestamp(self.__node, self.__delete_transaction)
        self.__comment_author_obj.rc_manabar.assert_rc_current_mana_is_reduced(delete_rc_cost, delete_timestamp)

    def assert_comment(self, mode: Literal["deleted", "not_deleted"]):
        voter = self.__create_comment_account()
        try:
            self.__wallet.api.vote(voter.name, self.__comment_author_obj.name, self.__comment_permlink, 1)
            vote_send = True
        except tt.exceptions.CommunicationError:
            vote_send = False

        if mode == "deleted":
            assert vote_send is False, "Vote send. Comment is not deleted"
        elif mode == "not_deleted":
            assert vote_send is True, "Vote not send. Comment is deleted"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")
