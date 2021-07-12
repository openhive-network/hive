#!/usr/bin/python3

from test_tools import *
from test_tools.communication import CommunicationError
from test_tools.wallet import Wallet
from test_tools.private.node import Node
from .shared_utilites import *

def list_proposals_by_creator(wallet : Wallet, creator_name : str) -> list:
  all_proposals = wallet.api.list_proposals(**get_list_proposal_args(creator_name))
  return find_proposals_by_creator_name(_creator=creator_name, _proposal_list=all_proposals)

def list_proposal_votes_by_voter(wallet : Wallet, voter_name : str):
  all_proposal_votes = wallet.api.list_proposal_votes(**get_list_proposal_votes_args(start=[voter_name]))
  return find_proposals_by_voter_name( voter_name, all_proposal_votes )

def test_create_proposal(wallet : Wallet, funded_account : funded_account_info, creator : Account):
  # check is input state empty
  creator_proposals_before_count = len(list_proposals_by_creator(wallet, creator.name))
  assert creator_proposals_before_count == 0

  # creates proposal
  prepared_proposal = prepare_proposal(funded_account)
  wallet.api.post_comment( **prepared_proposal.post_comment_arguments )
  wallet.api.create_proposal( **prepared_proposal.create_proposal_arguments )

  # final checks
  creator_proposals_after_count = len(list_proposals_by_creator(wallet, creator.name))
  assert creator_proposals_before_count + 1 == creator_proposals_after_count

def test_remove_proposal(node : Node, wallet : Wallet, funded_account : funded_account_info, creator : Account):
  # re-running previous test to get same enviroment
  test_create_proposal(wallet, funded_account, creator)

  # gathering id
  proposals_before = list_proposals_by_creator(wallet, creator.name)
  assert len(proposals_before) > 0
  first_proposal_id = proposals_before[0]['id']

  # adding one more proposal to check is proper one being deleted
  prepared_proposal = prepare_proposal(funded_account, prefix='dummy-', author_is_creator=False)
  wallet.api.post_comment( **prepared_proposal.post_comment_arguments )
  wallet.api.create_proposal( **prepared_proposal.create_proposal_arguments )

  # removing proposal from previous test
  wallet.api.remove_proposal(deleter=creator.name, ids=[first_proposal_id])

  # final checks
  proposals_after = list_proposals_by_creator(wallet, creator.name)
  assert len(proposals_after) + 1 == len(proposals_before)
  assert len([x for x in proposals_after if x['id'] == first_proposal_id]) == 0, "removed invalid proposal"

def test_update_proposal_votes(wallet : Wallet, funded_account : funded_account_info, creator : Account):
  # check is input state empty
  voter_proposals_before_count = len(list_proposal_votes_by_voter(wallet, creator.name))
  assert voter_proposals_before_count == 0

  # re-running first test to get same enviroment
  test_create_proposal(wallet, funded_account, creator)

  # gathering id from re-runned test
  proposals_before = list_proposals_by_creator(wallet, creator.name)
  assert len(proposals_before) > 0
  first_proposal_id = proposals_before[0]['id']

  # checking is created proposal is not already voted
  voter_proposals_inter_count = len(list_proposal_votes_by_voter(wallet, creator.name))
  assert voter_proposals_before_count == voter_proposals_inter_count

  # performs actual vote
  wallet.api.update_proposal_votes(voter=creator.name, proposals=[first_proposal_id], approve=True)

  # checks votes
  voter_proposals_after_count = len(list_proposal_votes_by_voter(wallet, creator.name))
  assert voter_proposals_before_count + 1 == voter_proposals_after_count

def test_create_proposal_fail_negative_payment(wallet : Wallet, funded_account : funded_account_info, creator : Account):

  ERROR_MESSAGE = "daily_pay.amount >= 0: Daily pay can't be negative value"
  exception = None

  assert len(list_proposals_by_creator(wallet, creator.name)) == 0

  prepared_proposal = prepare_proposal(funded_account)
  prepared_proposal.create_proposal_arguments["daily_pay"] = Asset.Tbd(-1) # "-1.000 TBD"
  wallet.api.post_comment( **prepared_proposal.post_comment_arguments )

  try:
    wallet.api.create_proposal( **prepared_proposal.create_proposal_arguments )
  except CommunicationError as e:
    exception = e

  assert exception is not None
  assert ERROR_MESSAGE in exception.response

  assert len(list_proposals_by_creator(wallet, creator.name)) == 0
