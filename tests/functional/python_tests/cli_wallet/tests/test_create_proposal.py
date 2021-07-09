#!/usr/bin/python3

from test_tools import *
from test_tools.wallet import Wallet
from .shared_utilites import funded_account_info, find_proposals_by_creator_name, prepare_proposal

def test_create_proposal(wallet : Wallet, funded_account : funded_account_info):
  creator = funded_account.creator
  actor = funded_account.account

  all_proposals_before = wallet.api.list_proposals(start=[creator.name], limit=50, order_by="by_creator", order_type="ascending", status="all")
  creator_proposals_before_count = len( find_proposals_by_creator_name(_creator=creator.name, _proposal_list=all_proposals_before) )
  logger.info(f'proposals before creating proposal: { creator_proposals_before_count }')
  assert creator_proposals_before_count == 0

  prepared_proposal = prepare_proposal(funded_account)
  wallet.api.post_comment( **prepared_proposal.post_comment_arguments )
  wallet.api.create_proposal( **prepared_proposal.create_proposal_arguments )

  all_proposals_after = wallet.api.list_proposals(start=[creator.name], limit=50, order_by="by_creator", order_type="ascending", status="all")
  creator_proposals_after_count = len( find_proposals_by_creator_name(_creator=creator.name, _proposal_list=all_proposals_after) )
  logger.info(f'proposals after creating proposal: { creator_proposals_after_count }')

  assert creator_proposals_before_count + 1 == creator_proposals_after_count
