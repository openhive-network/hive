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

def test_update_proposal(wallet : Wallet, funded_account : funded_account_info):
  from datetime import datetime as date_type
  def check_is_proposal_update_exists(block_number : int, end_date : date_type) -> bool:
    from time import sleep

    if not isinstance(end_date, str):
      end_date = format_datetime(end_date)

    TIME_TO_MAKE_BLOCKS_IRREVERRSIBBLE = (3 * 22)

    logger.info(f"Awaiting for making current reversible - irreversible: {TIME_TO_MAKE_BLOCKS_IRREVERRSIBBLE} s")
    sleep(TIME_TO_MAKE_BLOCKS_IRREVERRSIBBLE)
    logger.info("End of waiting")

    for bn in range(block_number, block_number + 5):
      logger.info(f"checking block: {bn}")
      response = wallet.api.get_ops_in_block(block_num=bn, only_virtual=False)
      if len(response['result']) > 0:
        logger.info(f'got_ops_in_block response: {response}')
        break

    op = response['result'][0]['op']
    extensions = op[1]['extensions'][0]
    return extensions[0] == 1 and extensions[1]['end_date'] == end_date

  author = funded_account.account
  current_daily_pay = Asset.Tbd(10)

  # creates proposal
  prepared_proposal = prepare_proposal(input=funded_account, author_is_creator=False)
  wallet.api.post_comment( **prepared_proposal.post_comment_arguments )


  prepared_proposal.create_proposal_arguments['daily_pay'] = current_daily_pay
  wallet.api.create_proposal( **prepared_proposal.create_proposal_arguments)

  # get proposal id
  proposals = list_proposals_by_creator(wallet=wallet, creator_name=author.name)
  assert len(proposals) > 0
  proposal_id = proposals[0]['id']

  # updating proposal
  current_daily_pay -= Asset.Tbd(1)
  update_args = {
    "proposal_id": proposal_id,
    "creator": author.name,
    "daily_pay": str(current_daily_pay),
    "subject": "updated subject",
    "permlink": prepared_proposal.permlink,
    "end_date": format_datetime(prepared_proposal.end_date - timedelta(days=2))
  }
  block = wallet.api.update_proposal(**update_args)['result']['ref_block_num'] + 1
  assert check_is_proposal_update_exists(block, update_args["end_date"])

  proposal = wallet.api.find_proposals([proposal_id])['result'][0]

  assert(proposal['daily_pay'] == current_daily_pay)
  assert(proposal['subject'] == update_args['subject'])
  assert(proposal['permlink'] == prepared_proposal.permlink)
  assert(proposal['end_date'] == update_args['end_date'])

  logger.info("testing updating the proposal without the end date")
  from copy import deepcopy
  last_date = deepcopy(update_args['end_date'])
  
  current_daily_pay -= Asset.Tbd(1)
  logger.info(current_daily_pay)
  update_args['daily_pay'] = str(current_daily_pay)
  update_args['subject'] = "updated subject again"
  update_args['end_date'] = None

  wallet.api.update_proposal(**update_args)
  proposal = wallet.api.find_proposals([proposal_id])['result'][0]

  assert(proposal['daily_pay'] == current_daily_pay)
  assert(proposal['subject'] == update_args['subject'])
  assert(proposal['permlink'] == prepared_proposal.permlink)
  assert(proposal['end_date'] == last_date)

