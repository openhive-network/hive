#!/usr/bin/python3

from test_tools import *
from test_tools.wallet import Wallet
from .shared_utilites import *
from .shared_utilites import prepared_proposal_data_with_id as proposal_data_t
from .conftest import create_funded_account
from datetime import datetime as dt

active_values = ["all", "inactive", "active", "expired", "votable"]
proposals_order_by_values = ["by_creator", "by_start_date", "by_end_date", "by_total_votes"]
votes_order_by_values = ["by_voter_proposal"]
order_type_values = ["ascending", "descending"]

def test_list_proposals(wallet: Wallet, creator_proposal_id : proposal_data_t, creator : Account):
  start_point_before_test = format_datetime(dt.now())

  name_order_by = [proposals_order_by_values[0]]
  date_order_by = [proposals_order_by_values[1], proposals_order_by_values[2]]
  value_order_by = [proposals_order_by_values[3]]

  for active in active_values:
    for order_type in order_type_values:
      for order_by in proposals_order_by_values:
        if order_by in date_order_by: start = [start_point_before_test]
        elif order_by in value_order_by: start = [0]
        else: start = [creator.name]

        wallet.api.list_proposals(
          **get_list_proposal_args(
            start=start,
            order_by=order_by,
            order_type=order_type,
            status=active
          )
        )


def test_list_voter_proposal(wallet : Wallet, creator_proposal_id : proposal_data_t, creator : Account):
  wallet.api.update_proposal_votes(voter=creator.name, proposals=[creator_proposal_id.id], approve=True)

  for active in active_values:
    for order_by in votes_order_by_values:
      for order_type in order_type_values:
        wallet.api.list_proposal_votes(
          **get_list_proposal_votes_args(
            start=[creator.name],
            order_by=order_by,
            order_type=order_type,
            status=active
          )
        )


def test_find_proposals(wallet : Wallet, creator : Account):
  ACCOUNTS_COUNT = 8
  PROPOSAL_ID_TEST_SCHEME = [[1], [2], [3], [1,2], [1,2,3], [2,3], [3,4], [4,5], [1,2,3,4,5,6,7]]

  def inline_create_proposal( input_account : funded_account_info ):
    data = prepare_proposal( input=input_account, author_is_creator=False )
    # with wallet.in_single_transaction():
    wallet.api.post_comment( **data.post_comment_arguments )
    wallet.api.create_proposal( **data.create_proposal_arguments )

  # create proposal for each account
  accounts = [ create_funded_account(creator = creator, wallet = wallet, id = i) for i in range(ACCOUNTS_COUNT) ]
  [ inline_create_proposal( input_account=acc ) for acc in accounts ]

  for proposal_test_case in PROPOSAL_ID_TEST_SCHEME:
    result = wallet.api.find_proposals(proposal_ids=proposal_test_case)["result"]
    assert len(result) == len(proposal_test_case), f"result: {result}"
    result_ids = [ item['id'] for item in result ]
    assert proposal_test_case == result_ids, f'expected: {proposal_test_case}\ngiven:{result_ids}'