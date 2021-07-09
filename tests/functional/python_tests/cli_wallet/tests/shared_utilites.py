from test_tools import Account, Asset
from random import randint

class funded_account_info:
  def __init__(self):
    self.creator : Account = None
    self.account : Account = None
    self.funded_TESTS : Asset.Test = None
    self.funded_TBD : Asset.Tbd = None
    self.funded_VESTS : Asset.Test = None

def format_datetime(input) -> str:
  return input.strftime('%Y-%m-%dT%H:%M:%S')

def find_proposals_by_creator_name(_creator, _proposal_list):
  proposals = []
  for rs in _proposal_list["result"]:
    if rs["creator"] == _creator:
      proposals.append(rs)
  return proposals

def find_proposals_by_voter_name(_voter, _proposal_list):
  proposals = []
  for rs in _proposal_list["result"]:
    if rs["voter"] == _voter:
      proposals.append(rs)
  return proposals

class prepared_proposal_data:
  def __init__(self):
    self.post_comment_arguments : dict = None
    self.create_proposal_arguments : dict = None
    self.permlink : str = None

def prepare_proposal( input : funded_account_info, prefix : str = "test-") -> prepared_proposal_data:
  from hashlib import md5

  hash_input = f'{randint(0, 9999)}{prefix}{input.account.private_key}{input.account.public_key}'
  PERMLINK = md5(hash_input.encode('utf-8')).hexdigest()
  result = prepared_proposal_data()

  result.post_comment_arguments = {
    "author": input.creator.name,
    "permlink": PERMLINK,
    "parent_author": "",
    "parent_permlink": f"{prefix}post-parent-permlink",
    "title": f"{prefix}post-title",
    "body": f"{prefix}post-body",
    "json": "{}"
  }

  from datetime import datetime, timedelta
  result.create_proposal_arguments = {
    "creator": input.creator.name,
    "receiver": input.account.name,
    "start_date": format_datetime( datetime.now() + timedelta(weeks=5) ),
    "end_date": format_datetime( datetime.now() + timedelta(weeks=10) ),
    "daily_pay": Asset.Tbd(1),
    "subject": f"{prefix}-create-proposal-subject",
    "permlink": PERMLINK
  }

  result.permlink = PERMLINK
  return result
