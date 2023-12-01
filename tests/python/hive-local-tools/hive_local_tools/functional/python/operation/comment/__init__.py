from __future__ import annotations

from typing import TYPE_CHECKING, Literal

import test_tools as tt
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
from schemas.transaction import TransactionLegacy

if TYPE_CHECKING:
    from schemas.fields.hive_int import HiveInt

from hive_local_tools.functional.python.operation import (
    Account,
    create_transaction_with_any_operation,
    get_reward_hbd_balance,
    get_reward_vesting_balance,
    get_transaction_timestamp,
    get_virtual_operations,
)


class CommentTransaction(TransactionLegacy):
    operations: tuple[LegacyRepresentation[CommentOperation]] | tuple[
        LegacyRepresentation[CommentOperation], LegacyRepresentation[CommentOptionsOperationLegacy]
    ]
    rc_cost: int


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
        self.__comment_option_transaction = None
        self.__deleted: bool = False
        self.__beneficiaries: list[dict] = []
        self.__benefactors: list[str] = []

    @property
    def node(self):
        return self.__node

    @property
    def wallet(self):
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
        return self._benefactors

    def append_beneficiares(self, beneficiaries: list, beneficiary_accounts: list) -> None:
        self._beneficiaries = beneficiaries
        self._benefactors = beneficiary_accounts

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
        self,
        reply_type: Literal["no_reply", "reply_own_comment", "reply_another_comment"],
        **comment_options,
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

    def options(self, **comment_options):
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

    def assert_rc_mana_after_change_comment_options(self, mode: Literal["decrease", "is_unchanged"]):
        if mode == "decrease":
            comment_option_rc_cost = int(self.__comment_option_transaction["rc_cost"])
            comment_option_timestamp = get_transaction_timestamp(self.__node, self.__comment_option_transaction)
            self.__author.rc_manabar.assert_rc_current_mana_is_reduced(comment_option_rc_cost, comment_option_timestamp)
        elif mode == "is_unchanged":
            self.__author.rc_manabar.assert_current_mana_is_unchanged()
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")

    def __options(self, **comment_options):
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
            for beneficiary in comment_options["beneficiaries"]:
                beneficiary["weight"] = beneficiary["weight"] * 100

            comment_options["extensions"] = [
                [
                    "comment_payout_beneficiaries",
                    {"beneficiaries": comment_options["beneficiaries"]},
                ]
            ]
            comment_options.pop("beneficiaries")

        if "percent_hbd" in comment_options:
            comment_options["percent_hbd"] = comment_options["percent_hbd"] * 100

        return create_transaction_with_any_operation(
            self.__wallet,
            CommentOptionsOperationLegacy(
                author=self.__author.name,
                permlink=self.__permlink,
                **comment_options,
            ),
        )

    def assert_options_are_apply(self):
        comment_content = self.__node.api.database.find_comments(comments=[[self.author, self.__permlink]]).comments[0]

        for key in ["max_accepted_payout", "percent_hbd", "allow_votes", "allow_curation_rewards", "beneficiaries"]:
            if key in self.__comment_options and key != "max_accepted_payout":
                assert self.__comment_options[key] == getattr(comment_content, key), f"{key} is not apply"
            elif key in self.__comment_options and key == "max_accepted_payout":
                assert (
                    tt.Asset.from_legacy(self.__comment_options[key]).amount == getattr(comment_content, key).amount
                ), f"{key} is not apply"

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
        raise ValueError

    def assert_author_reward_virtual_operation(self, mode: Literal["generated", "not_generated"]):
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

    def assert_comment_benefactors_reward_virtual_operations(self, mode: Literal["generated", "not_generated"]):
        comment_benefactors_operations = get_reward_operations(self.__node, "comment_benefactor")
        for beneficiary in self._beneficiaries:
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

    def get_author_reward(self, mode: Literal["hbd_payout", "vesting_payout"]):
        assert mode in ["hbd_payout", "vesting_payout"], f"Unexpected value for 'mode': '{mode}'"
        author_reward_operations = get_reward_operations(self.__node, "author")
        for value in author_reward_operations:
            if value.author == self.author and value.permlink == self.permlink:
                return getattr(value, mode)
        raise ValueError("Comment not have generated author_reward_operation")

    def get_comment_benefactor_reward(self, benefactor: str, mode: Literal["hbd_payout", "vesting_payout"]):
        assert mode in ["hbd_payout", "vesting_payout"], f"Unexpected value for 'mode': '{mode}'"
        comment_benefactors_reward_operations = get_reward_operations(self.__node, "comment_benefactor")
        for value in comment_benefactors_reward_operations:
            if value.benefactor == benefactor and value.permlink == self.permlink:
                return getattr(value, mode)
        raise ValueError("Comment not have generated comment_benefactor_reward_operation")

    def __convert_hbd_to_hive(self, hbd: tt.Asset.Hbd) -> tt.Asset.Hive:
        hbd_to_hive_feed = self.__node.api.database.get_current_price_feed()
        hbd_to_hive_feed = hbd_to_hive_feed["base"].as_float() / hbd_to_hive_feed["quote"].as_float()
        return tt.Asset.Hive(hbd.as_float() / 1000 * hbd_to_hive_feed)

    def __convert_vesting_to_hive(self, vesting: tt.Asset.Vest) -> tt.Asset.Hive:
        gdgp = self.__node.api.database.get_dynamic_global_properties()
        total_vesting_shares = gdgp.total_vesting_shares
        total_vesting_fund_hive = gdgp.total_vesting_fund_hive
        vesting_to_hive_feed = (total_vesting_shares.as_float() / 1000000) / (total_vesting_fund_hive.as_float() / 1000)
        return tt.Asset.Hive(vesting.as_float() / 1000000 / vesting_to_hive_feed)

    def assert_resource_percentage_in_author_reward(self, hbd_percentage: int, vesting_percentage: int):
        hbd_reward = self.get_author_reward("hbd_payout")
        hbd_reward_as_hive = self.__convert_hbd_to_hive(hbd_reward)
        vesting_payout = self.get_author_reward("vesting_payout")
        vesting_payout_as_hive = self.__convert_vesting_to_hive(vesting_payout)
        assert (
            abs(hbd_reward_as_hive.as_float() * vesting_percentage - vesting_payout_as_hive.as_float() * hbd_percentage)
            <= 100
        ), "Resources percentage in reward are incorrect"

    def assert_resource_percentage_in_comment_benefactor_reward(
        self, hbd_percentage: int, vesting_percentage: int, benefactor: str | None = None
    ):
        if benefactor is None:
            benefactor = self._benefactors[0].name
        hbd_reward = self.get_comment_benefactor_reward(benefactor, "hbd_payout")
        hbd_reward_as_hive = self.__convert_hbd_to_hive(hbd_reward)
        vesting_payout = self.get_comment_benefactor_reward(benefactor, "vesting_payout")
        vesting_payout_as_hive = self.__convert_vesting_to_hive(vesting_payout)
        assert (
            abs(hbd_reward_as_hive.as_float() * vesting_percentage - vesting_payout_as_hive.as_float() * hbd_percentage)
            <= 100
        ), "Resources percentage in reward are incorrect"


class Vote:
    account_counter = 0

    def __init__(self, comment_obj: Comment, voter: Literal["random", "same_as_comment"] | CommentAccount):
        self.__comment_obj = comment_obj
        self.__voter = voter
        self.__vote_transaction: TransactionLegacy | None = None
        if isinstance(voter, CommentAccount):
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
                operation_values = operation_values + [operation.op["value"] for operation in operations]  # PROBLEM
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
            tt.logger.info(f"z operacji -   {voter_and_comment_permlink}")
            tt.logger.info(f"z objektu vote-   {(self.__voter, self.__comment_obj.permlink)}")
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
            raise ValueError(f"Downvote with weight {weight} is not allowed. Weight can take (0-100)")
        self.__update_account_info_and_execute_vote(weight)

    def assert_curation_reward_virtual_operation(self, mode: Literal["generated", "not_generated"]):
        curation_reward_operations = get_reward_operations(self.__comment_obj.node, "curation")
        curator_and_comment_permlink = [
            (value["curator"], value["comment_permlink"]) for value in curation_reward_operations
        ]
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

    def get_curation_vesting_reward(self):
        curation_reward_operations = get_reward_operations(self.__comment_obj.node, "curation")
        for value in curation_reward_operations:
            if value["curator"] == self.__voter.name and value["comment_permlink"] == self.__comment_obj.permlink:
                return value.reward
        raise ValueError("Vote not have generated curation_reward_operation")


class CommentAccount(Account):
    def __init__(self, name, node, wallet):
        super().__init__(_name=name, _node=node, _wallet=wallet)

    def __get_rewards_from_reward_operation(
        self,
        node,
        mode: Literal["author", "curation", "comment_benefactor"],
        reward_type: Literal["vesting_payout", "hbd_payout"],
    ):
        reward_operations = get_reward_operations(node, mode)
        if mode == "author" and reward_type is not None:
            return [getattr(vop, reward_type) for vop in reward_operations if vop.author == self.name]
        if mode == "comment_benefactor" and reward_type is not None:
            return [getattr(vop, reward_type) for vop in reward_operations if vop.benefactor == self.name]
        if mode == "curation" and reward_type == "vesting_payout":
            return [vop.reward for vop in reward_operations if vop.curator == self.name]
        raise ValueError(f"Wrong argument combination: 'mode': '{mode} and 'reward_type': {reward_type}")

    def assert_reward_hbd_balance(self, node):
        account_reward_hbd_balance = get_reward_hbd_balance(node, self.name)

        account_hbd_rewards_from_author_reward_operation = self.__get_rewards_from_reward_operation(
            node, "author", "hbd_payout"
        )
        if account_hbd_rewards_from_author_reward_operation == []:
            account_hbd_rewards_from_author_reward_operation.append(tt.Asset.Tbd(0))

        account_hbd_rewards_from_comment_benefactor_reward_operation = self.__get_rewards_from_reward_operation(
            node, "comment_benefactor", "hbd_payout"
        )
        if account_hbd_rewards_from_comment_benefactor_reward_operation == []:
            account_hbd_rewards_from_comment_benefactor_reward_operation.append(tt.Asset.Tbd(0))

        assert account_reward_hbd_balance == sum(account_hbd_rewards_from_author_reward_operation) + sum(
            account_hbd_rewards_from_comment_benefactor_reward_operation
        )

    def assert_reward_vesting_balance(self, node):
        account_reward_vesting_balance = get_reward_vesting_balance(node, self.name)

        account_vesting_rewards_from_author_reward_operation = self.__get_rewards_from_reward_operation(
            node, "author", "vesting_payout"
        )
        if account_vesting_rewards_from_author_reward_operation == []:
            account_vesting_rewards_from_author_reward_operation.append(tt.Asset.Vest(0))

        account_vesting_rewards_from_comment_benefactor_reward_operation = self.__get_rewards_from_reward_operation(
            node, "comment_benefactor", "vesting_payout"
        )
        if account_vesting_rewards_from_comment_benefactor_reward_operation == []:
            account_vesting_rewards_from_comment_benefactor_reward_operation.append(tt.Asset.Vest(0))

        account_vesting_rewards_from_curation_reward_operation = self.__get_rewards_from_reward_operation(
            node, "curation", "vesting_payout"
        )
        if account_vesting_rewards_from_curation_reward_operation == []:
            account_vesting_rewards_from_curation_reward_operation.append(tt.Asset.Vest(0))

        assert account_reward_vesting_balance == sum(account_vesting_rewards_from_author_reward_operation) + sum(
            account_vesting_rewards_from_comment_benefactor_reward_operation
        ) + sum(account_vesting_rewards_from_curation_reward_operation)

    def assert_curation_reward_virtual_operation(self, mode: Literal["generated", "not_generated"]):
        curation_reward_operations = get_reward_operations(self.node, "curation")
        curator = [value["curator"] for value in curation_reward_operations]
        if mode == "generated":
            assert self.name in curator, "Curation_reward_operation not generated, but it should have been"
        elif mode == "not_generated":
            assert self.name not in curator, "Curation_reward_operation generated, but it should have not been"
        else:
            raise ValueError(f"Unexpected value for 'mode': '{mode}'")
