#!/usr/bin/python3

from os import stat
import pytest
from test_tools import *
from test_tools.wallet import Wallet
from .shared_utilites import funded_account_info, find_proposals_by_voter_name, prepare_proposal, find_proposals_by_creator_name

def test_update_proposal_votes(wallet : Wallet, funded_account : funded_account_info):

  creator = funded_account.creator
  actor = funded_account.account

  all_proposal_votes_begin = wallet.api.list_proposal_votes(start=[creator.name], limit=50, order_by="by_voter_proposal", order_type="ascending", status="all")
  voter_proposals_before_count = len( find_proposals_by_voter_name( creator.name, all_proposal_votes_begin ) )
  assert voter_proposals_before_count == 0

  prepared_proposal = prepare_proposal(funded_account)
  wallet.api.post_comment( **prepared_proposal.post_comment_arguments )
  wallet.api.create_proposal( **prepared_proposal.create_proposal_arguments )

  all_proposals_inter = wallet.api.list_proposals(start=[creator.name], limit=50, order_by="by_creator", order_type="ascending", status="all")
  proposals = find_proposals_by_creator_name(creator.name, all_proposals_inter)
  assert len(proposals) == 1
  created_proposal_id = proposals[0]["id"]
  assert created_proposal_id == 0

  all_proposal_votes_inter = wallet.api.list_proposal_votes(start=[creator.name], limit=50, order_by="by_voter_proposal", order_type="ascending", status="all")
  voter_proposals_inter_count = len( find_proposals_by_voter_name( creator.name, all_proposal_votes_inter ) )
  assert voter_proposals_before_count == voter_proposals_inter_count

  wallet.api.update_proposal_votes(voter=creator.name, proposals=[created_proposal_id], approve=True)

  all_proposal_votes_after = wallet.api.list_proposal_votes(start=[creator.name], limit=50, order_by="by_voter_proposal", order_type="ascending", status="all")
  voter_proposals_after_count = len( find_proposals_by_voter_name( creator.name, all_proposal_votes_after ) )
  assert voter_proposals_before_count + 1 == voter_proposals_after_count
