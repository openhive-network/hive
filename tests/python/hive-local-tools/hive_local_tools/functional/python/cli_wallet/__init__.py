from __future__ import annotations

from datetime import datetime, timedelta
from random import randint

import test_tools as tt


class FundedAccountInfo:
    def __init__(self):
        self.creator: tt.Account = None
        self.account: tt.Account = None
        self.funded_TESTS: tt.Asset.Test = None
        self.funded_TBD: tt.Asset.Tbd = None
        self.funded_VESTS: tt.Asset.Test = None


def format_datetime(input) -> str:
    return input.strftime("%Y-%m-%dT%H:%M:%S")


def find_proposals_by_creator_name(_creator, _proposal_list):
    proposals = []
    for rs in _proposal_list:
        if rs["creator"] == _creator:
            proposals.append(rs)
    return proposals


def find_proposals_by_voter_name(_voter, _proposal_list):
    proposals = []
    for rs in _proposal_list:
        if rs["voter"] == _voter:
            proposals.append(rs)
    return proposals


class PreparedProposalData:
    def __init__(self):
        self.post_comment_arguments: dict = None
        self.create_proposal_arguments: dict = None
        self.permlink: str = None
        self.start_date: datetime = None
        self.end_date: datetime = None


class PreparedProposalDataWithId(PreparedProposalData):
    def __init__(self, base: PreparedProposalData, id: int | None = None):
        super().__init__()
        self.id: int = id
        self.permlink = base.permlink
        self.create_proposal_arguments = base.create_proposal_arguments
        self.post_comment_arguments = base.post_comment_arguments
        self.start_date = base.start_date
        self.end_date = base.end_date


def argument_as_list(item) -> list:
    return item if isinstance(item, list) else [item]


def get_listing_args(limit: int = 50, order_by: str = "by_creator", order_type: str = "ascending", status: str = "all"):
    return {"limit": limit, "order_by": order_by, "order_type": order_type, "status": status}


def get_list_proposal_args(start: list, **kwargs):
    result = get_listing_args(**kwargs)
    result["start"] = argument_as_list(start)
    return result


def get_list_proposal_votes_args(start: list, **kwargs):
    if "order_by" not in kwargs:
        kwargs["order_by"] = "by_voter_proposal"
    result = get_listing_args(**kwargs)
    result["start"] = argument_as_list(start)
    return result


def prepare_proposal(
    input: FundedAccountInfo, prefix: str = "test-", author_is_creator: bool = True
) -> PreparedProposalData:
    from hashlib import md5

    creator: tt.Account = input.creator if author_is_creator else input.account
    hash_input = f"{randint(0, 9999)}{prefix}{creator.private_key}{creator.public_key}{creator.name}"
    permlink = md5(hash_input.encode("utf-8")).hexdigest()
    result = PreparedProposalData()

    result.post_comment_arguments = {
        "author": creator.name,
        "permlink": permlink,
        "parent_author": "",
        "parent_permlink": f"{prefix}post-parent-permlink",
        "title": f"{prefix}post-title",
        "body": f"{prefix}post-body",
        "json": "{}",
    }

    result.start_date = datetime.now() + timedelta(weeks=5)
    result.end_date = datetime.now() + timedelta(weeks=10)

    result.create_proposal_arguments = {
        "creator": creator.name,
        "receiver": input.account.name,
        "start_date": format_datetime(result.start_date),
        "end_date": format_datetime(result.end_date),
        "daily_pay": tt.Asset.Tbd(1),
        "subject": f"{prefix}-create-proposal-subject",
        "permlink": permlink,
    }

    result.permlink = permlink
    return result


def print_test_name(fun, *args, **kwargs):
    def print_test_name_impl():  # (fun, *args, **kwargs):
        print(f"starting test case: {fun.__name__}")
        return fun(*args, **kwargs)

    return print_test_name_impl


def create_funded_account(creator: tt.Account, wallet: tt.Wallet, id: int = 0) -> FundedAccountInfo:
    result = FundedAccountInfo()

    result.creator = creator

    result.account = tt.Account(f"actor-{id}")
    result.funded_TESTS = tt.Asset.Test(20)
    result.funded_TBD = tt.Asset.Tbd(20)
    result.funded_VESTS = tt.Asset.Test(200)

    tt.logger.info("importing key to wallet")
    wallet.api.import_key(result.account.private_key)
    tt.logger.info(f"imported key: {result.account.private_key} for account: {result.account.name}")

    with wallet.in_single_transaction():
        tt.logger.info("creating actor with keys")
        wallet.api.create_account_with_keys(
            creator=creator.name,
            newname=result.account.name,
            json_meta="{}",
            owner=result.account.public_key,
            active=result.account.public_key,
            posting=result.account.public_key,
            memo=result.account.public_key,
        )

        tt.logger.info("transferring TESTS")
        wallet.api.transfer(from_=creator.name, to=result.account.name, amount=result.funded_TESTS, memo="TESTS")

        tt.logger.info("transferring TBD")
        wallet.api.transfer(from_=creator.name, to=result.account.name, amount=result.funded_TBD, memo="TBD")

    tt.logger.info("transfer to VESTing")
    wallet.api.transfer_to_vesting(from_=creator.name, to=result.account.name, amount=result.funded_VESTS)

    return result
