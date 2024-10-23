from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.cli_wallet import (
    FundedAccountInfo,
    PreparedProposalDataWithId,
    create_funded_account,
    prepare_proposal,
)


@pytest.fixture()
def wallet(node) -> tt.OldWallet:
    return tt.OldWallet(attach_to=node)


@pytest.fixture()
def creator(node) -> tt.Account:
    return tt.Account("initminer")


@pytest.fixture()
def funded_account(creator: tt.Account, wallet: tt.OldWallet, id: int = 0) -> FundedAccountInfo:
    return create_funded_account(creator=creator, wallet=wallet, id=id)


def create_proposal(wallet: tt.OldWallet, funded_account: FundedAccountInfo, creator_is_propsal_creator: bool):
    prepared_proposal = prepare_proposal(funded_account, "initial", author_is_creator=creator_is_propsal_creator)
    wallet.api.post_comment(**prepared_proposal.post_comment_arguments)
    wallet.api.create_proposal(**prepared_proposal.create_proposal_arguments)
    response = wallet.api.list_proposals(
        start=[""], limit=50, order_by="by_creator", order_type="ascending", status="all"
    )
    for prop in response:
        if prop["permlink"] == prepared_proposal.permlink:
            return PreparedProposalDataWithId(base=prepared_proposal, id=prop["id"])

    raise AssertionError("proposal not found")


@pytest.fixture()
def creator_proposal_id(wallet: tt.OldWallet, funded_account: FundedAccountInfo) -> PreparedProposalDataWithId:
    return create_proposal(wallet=wallet, funded_account=funded_account, creator_is_propsal_creator=True)


@pytest.fixture()
def account_proposal_id(wallet: tt.OldWallet, funded_account: FundedAccountInfo) -> PreparedProposalDataWithId:
    return create_proposal(wallet=wallet, funded_account=funded_account, creator_is_propsal_creator=False)
